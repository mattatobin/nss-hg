# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
{
  'includes': [
    '../../coreconf/config.gypi'
  ],
  'targets': [
    {
      'target_name': 'lib_certhigh_exports',
      'type': 'none',
      'copies': [
        {
          'files': [
            'ocsp.h',
            'ocspt.h'
          ],
          'destination': '<(nss_dist_dir)/public/<(module)'
        },
        {
          'files': [
            'ocspi.h',
            'ocspti.h'
          ],
          'destination': '<(nss_dist_dir)/private/<(module)'
        }
      ]
    }
  ],
  'variables': {
    'module': 'nss'
  }
}
