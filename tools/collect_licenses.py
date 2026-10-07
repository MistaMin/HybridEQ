#!/usr/bin/env python3
"""Copies the license texts of bundled third-party code into Licenses/third-party.
Run after changing the JUCE version (needs a configured build in build3/ so the sources are present)."""
import glob, os, re, shutil, sys
root = os.path.join(os.path.dirname(__file__), '..')
D = os.path.join(root, 'build3/_deps')
J = os.path.join(D, 'juce-src/modules')
out = os.path.join(root, 'Licenses/third-party')
shutil.rmtree(out, ignore_errors=True); os.makedirs(out)

def find(pattern):
    hits = glob.glob(pattern)
    if not hits: sys.exit('missing: ' + pattern)
    return hits[0]
def cp(src, name): shutil.copyfile(find(src), os.path.join(out, name))

vst3 = 'juce_audio_processors*/format_types/VST3_SDK'
cp(f'{J}/{vst3}/LICENSE.txt', 'VST3-SDK-LICENSE-MIT.txt')
for n in ['lv2', 'lilv', 'serd', 'sord', 'sratom']:
    cp(f'{J}/juce_audio_processors*/format_types/LV2_SDK/{n}/COPYING', f'LV2-{n}-COPYING.txt')
cp(D + '/clap_juce_extensions-src/clap-libs/clap/LICENSE', 'CLAP-LICENSE.txt')
cp(D + '/clap_juce_extensions-src/clap-libs/clap-helpers/LICENSE', 'CLAP-helpers-LICENSE.txt')
cp(D + '/clap_juce_extensions-src/LICENSE.md', 'clap-juce-extensions-LICENSE.txt')
cp(J + '/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt', 'AudioUnitSDK-LICENSE-Apache-2.0.txt')
cp(J + '/juce_audio_formats/codecs/flac/Flac Licence.txt', 'FLAC-Licence.txt')
cp(J + '/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt', 'OggVorbis-Licence.txt')
cp(J + '/juce_audio_formats/codecs/oggvorbis/libvorbis-*/COPYING', 'libvorbis-COPYING.txt')
cp(J + '/juce_graphics/image_formats/pnglib/LICENSE', 'libpng-LICENSE.txt')
cp(J + '/juce_graphics/image_formats/jpglib/README', 'libjpeg-IJG-README.txt')
cp(J + '/juce_graphics/fonts/harfbuzz/COPYING', 'HarfBuzz-COPYING.txt')
cp(J + '/juce_graphics/unicode/sheenbidi/LICENSE', 'SheenBidi-LICENSE.txt')
zh = open(find(J + '/juce_core/zip/zlib/zlib.h')).read()
open(os.path.join(out, 'zlib-LICENSE.txt'), 'w').write(re.search(r'/\* zlib\.h.*?\*/', zh, re.S).group(0) + '\n')
# VST3 usage guidelines (trademark / logo rules) shipped with the SDK
usage = glob.glob(f'{J}/{vst3}/VST3_Usage_Guidelines.pdf')
if usage: shutil.copyfile(usage[0], os.path.join(out, 'VST3-Usage-Guidelines.pdf'))
open(os.path.join(out, 'JUCE-NOTICE.txt'), 'w').write('''JUCE
HybridEQ is built with the JUCE framework (https://juce.com). The official HybridEQ binaries are built under
the publisher's JUCE commercial license; end users do not need a JUCE license to install or use them.
JUCE is not covered by the MIT licenses of HybridEQ and GoodLookinUI. Anyone building their own version
must comply with the JUCE license that applies to their build (https://juce.com/legal/juce-8-licence/).
Libraries bundled inside JUCE keep their own licenses; the ones compiled into HybridEQ are reproduced in
this folder: zlib, libpng, libjpeg (IJG), FLAC, Ogg Vorbis, HarfBuzz and SheenBidi. The plug-in format
SDKs are also reproduced here: VST3 (MIT), LV2 (and lilv/serd/sord/sratom), CLAP and Audio Unit.
''')
open(os.path.join(out, 'VST-TRADEMARK-NOTICE.txt'), 'w').write('''VST is a trademark of Steinberg Media Technologies GmbH, registered in Europe and other countries.
The VST3 plug-in format uses the Steinberg VST3 SDK (MIT license); see VST3-SDK-LICENSE-MIT.txt and
VST3-Usage-Guidelines.pdf. CLAP is an open plug-in standard (https://cleveraudio.org); LV2 is an open
plug-in standard (https://lv2plug.in). Audio Units is a technology of Apple Inc.; AAX is a technology of
Avid Technology, Inc.
''')
print(len(os.listdir(out)), 'files in', out)
