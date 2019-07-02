#!/bin/sh
#
#
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#
# Determine last git revision containing an actual change on a given branch
# Usage: lastchange.sh gitpath [-d default_file] [-o out_file]
set -e
set -u

SVN_PATH=$1
shift 1
DEFAULT_FILE=
OUT_FILE=/dev/stdout

while [ $# -ge 2 ]; do
  case $1 in
  -d)
    # -d has no effect if file doesn't exist.
    if [ -f $2 ]; then
      DEFAULT_FILE=$2
    fi
    shift 2
    ;;
  -o)
    OUT_FILE=$2
    shift 2
    ;;
  *)
    echo "Usage: lastchange.sh gitpath [-d default_file] [-o out_file]"
    exit 1
    ;;
  esac
done

if [ -z $DEFAULT_FILE ]; then
  KEY='Last Changed Rev: '
  REVISION=$(git rev-list --all --count)
  echo LASTCHANGE=$REVISION > $OUT_FILE
else
  cat $DEFAULT_FILE > $OUT_FILE
fi
