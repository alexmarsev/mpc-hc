# (C) 2015 see Authors.txt
#
# This file is part of MPC-HC.
#
# MPC-HC is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# MPC-HC is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import sys
import glob

from TranslationDataRC import *


def UpdateShell(normalizePOFile=True):
    filenameBase = r'..\shellres\shellres-en.rc2'
    with codecs.open(r'..\shellres\shellres-translations.rc2', 'w', 'utf16') as fOut:
        for cfgPath in glob.glob(r'cfg\*.cfg'):
            config = ConfigParser.RawConfigParser()
            config.readfp(codecs.open(cfgPath, 'r', 'utf8'))
            poPath = r'PO\shellres.' + config.get('Info', 'langShortName')

            translationData = TranslationDataRC()
            translationData.loadFromPO(poPath, 'po', (False, False, True))

            with codecs.open(filenameBase, 'r', detectEncoding(filenameBase)) as fBase:
                for line in fBase:
                    if translationData.stringTableStart.match(line):
                        fOut.write(line)
                        translationData.translateRCStringTable(fBase, fOut)
                    elif line == u'LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US\r\n':
                        fOut.write(u'LANGUAGE ' + config.get('Info', 'langDefine') + '\r\n')
                    else:
                        fOut.write(line)

            if normalizePOFile:
                # Write back the PO file to ensure it's properly normalized
                translationData.writePO(poPath, 'po', (False, False, True))

if __name__ == '__main__':
    UpdateShell()
