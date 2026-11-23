#!/usr/bin/env python3
"""Build the versioned HybridEQ PDF manual from Docs/UserManual.md (requires reportlab)."""
from pathlib import Path
import re
from xml.sax.saxutils import escape
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.utils import ImageReader
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Image, Table, TableStyle, PageBreak

ROOT = Path(__file__).resolve().parents[1]
VERSION = (ROOT / "VERSION").read_text().strip()
SOURCE = ROOT / "Docs/UserManual.md"
OUTPUT = ROOT / "output/pdf/HybridEQ-User-Manual.pdf"

def inline(text):
    text = escape(text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"`([^`]+)`", r"<font name='Courier'>\1</font>", text)
    return re.sub(r"\[([^\]]+)\]\((https?://[^)]+)\)", r'<link href="\2" color="#146953">\1</link>', text)

def main():
    text = SOURCE.read_text(encoding="utf-8")
    if f"version {VERSION}" not in text:
        raise SystemExit("Manual version must match VERSION")
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    styles = getSampleStyleSheet()
    body = ParagraphStyle("ManualBody", fontName="Helvetica", fontSize=9.2, leading=13, textColor=colors.HexColor("#263238"), spaceAfter=8)
    cell = ParagraphStyle("ManualCell", parent=body, fontSize=8, leading=11, spaceAfter=0)
    head = ParagraphStyle("ManualHead", parent=body, fontName="Helvetica-Bold", fontSize=21, leading=25, textColor=colors.HexColor("#146953"), spaceAfter=14, keepWithNext=True)
    sub = ParagraphStyle("ManualSub", parent=head, fontSize=13, leading=17, spaceAfter=10)
    bullet = ParagraphStyle("ManualBullet", parent=body, leftIndent=11, firstLineIndent=-8, bulletIndent=0, spaceAfter=5)
    width = A4[0] - 88
    story = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if not line:
            i += 1
            continue
        if line == "<!-- pagebreak -->":
            story.append(PageBreak()); i += 1; continue
        if line.startswith("!["):
            target = re.fullmatch(r"!\[[^\]]*\]\(([^)]+)\)", line)
            path = SOURCE.parent / target.group(1)
            w, h = ImageReader(str(path)).getSize()
            story.extend([Image(str(path), width=width, height=width*h/w), Spacer(1,12)])
            i += 1; continue
        if line.startswith("|"):
            rows = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                parts = [v.strip() for v in lines[i].strip().strip("|").split("|")]
                if not all(re.fullmatch(r"[-: ]+", v) for v in parts):
                    rows.append([Paragraph(inline(v),cell) for v in parts])
                i += 1
            n = len(rows[0])
            widths = ([90,width-90] if n == 2 else [width/n]*n)
            table = Table(rows,colWidths=widths,repeatRows=1,hAlign="LEFT")
            table.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,0),colors.HexColor("#e5f0ec")),("GRID",(0,0),(-1,-1),0.35,colors.HexColor("#c8d5d0")),("VALIGN",(0,0),(-1,-1),"TOP"),("LEFTPADDING",(0,0),(-1,-1),7),("RIGHTPADDING",(0,0),(-1,-1),7),("TOPPADDING",(0,0),(-1,-1),6),("BOTTOMPADDING",(0,0),(-1,-1),6)]))
            story.extend([table,Spacer(1,12)]); continue
        if line.startswith("# "):
            story.append(Paragraph(inline(line[2:]),head)); i += 1; continue
        if line.startswith("## "):
            story.append(Paragraph(inline(line[3:]),sub)); i += 1; continue
        if line.startswith("- "):
            story.append(Paragraph("- "+inline(line[2:]),bullet)); i += 1; continue
        paragraph = [line]; i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith(("#","|","- ","![","<!--")):
            paragraph.append(lines[i].strip()); i += 1
        story.append(Paragraph(inline(" ".join(paragraph)),body))
    def footer(canvas, doc):
        canvas.saveState(); canvas.setFont("Helvetica",8); canvas.setFillColor(colors.HexColor("#60746d"))
        canvas.drawString(44,27,f"OpenGrid / HybridEQ {VERSION}")
        canvas.drawRightString(A4[0]-44,27,str(doc.page)); canvas.restoreState()
    doc = SimpleDocTemplate(str(OUTPUT),pagesize=A4,rightMargin=44,leftMargin=44,topMargin=40,bottomMargin=45,title=f"HybridEQ {VERSION} User Manual",author="OpenGrid",creator="HybridEQ documentation")
    doc.build(story,onFirstPage=footer,onLaterPages=footer)
    print(OUTPUT)

if __name__ == "__main__":
    main()
