#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Usage: ./system_test.sh HOSTNAME
# Tests a mod_pagespeed installation by fetching and verifying all the examples.
# Exits with status 0 if all tests pass.  Exits 1 immediately if any test fails.

if [ $# != 1 ]; then
  echo Usage: ./system_test.sh HOSTNAME
  exit 2
fi;

HOSTNAME=$1

EXAMPLE_ROOT=http://$HOSTNAME/mod_pagespeed_example
STATISTICS_URL=http://localhost/mod_pagespeed_statistics
BAD_RESOURCE_URL=http://$HOSTNAME/mod_pagespeed/ic.a.bad.css

OUTFILE=/tmp/mod_pagespeed_test/fetched_example.html
OUTDIR=/tmp/mod_pagespeed_test/fetched_directory


# Wget is used three different ways.  The first way is nonrecursive and dumps a
# single page (with headers) to standard out.  This is useful for grepping for a
# single expected string that's the result of a first-pass rewrite:
#   wget -q -O --save-headers - $URL | grep -q foo
# "-q" quells wget's noisy output; "-O -" dumps to stdout; grep's -q quells
# its output and uses the return value to indicate whether the string was
# found.  Note that exiting with a nonzero value will immediately kill
# the make run.
#
# Sometimes we want to check for a condition that's not true on the first dump
# of a page, but becomes true after a few seconds as the server's asynchronous
# fetches complete.  For this we use the the fetch_until() function:
#   fetch_until $URL 'grep -c delayed_foo' 1
# In this case we will continuously fetch $URL and pipe the output to
# grep -c (which prints the count of matches); we repeat until the number is 1.
#
# The final way we use wget is in a recursive mode to download all prerequisites
# of a page.  This fetches all resources associated with the page, and thereby
# validates the resources generated by mod_pagespeed:
#   wget -H -p -q -nd -P $OUTDIR $EXAMPLE_ROOT/$FILE
# Here -H allows wget to cross hosts (e.g. in the case of a sharded domain); -p
# means to fetch all prerequisites; -nd puts all results in one directory; -P
# specifies that directory.  We can then run commands on $OUTDIR/$FILE
# and nuke $OUTDIR when we're done.

WGET_DUMP="wget -q -O - --save-headers"
WGET_PREREQ="wget -H -p -q -nd -P $OUTDIR"

# Call with a command and its args.  Echos the command, then tries to eval it.
# If it returns false, fail the tests.
function check() {
  echo "     " $@
  if eval "$@"; then
    echo PASS.
  else
    echo FAIL.
    exit 1;
  fi;
}

# Continously fetches URL and pipes the output to COMMAND.  Loops until
# COMMAND outputs RESULT, in which case we return 0, or until 10 seconds have
# passed, in which case we return 1.
function fetch_until() {
  URL=$1
  COMMAND=$2
  RESULT=$3

  TIMEOUT=10
  START=`date +%s`
  STOP=$((START+$TIMEOUT))

  echo "     " Fetching $URL until '`'$COMMAND'`' = $RESULT
  while test -t; do
    if [ `wget -q -O - $URL 2>&1 | $COMMAND` = $RESULT ]; then
      /bin/echo "PASS."
      return;
    fi;
    if [ `date +%s` -gt $STOP ]; then
      /bin/echo "FAIL."
      exit 1;
    fi;
    /bin/echo -n "."
    sleep 0.1
  done;
}


# General system tests

echo TEST: mod_pagespeed is running in Apache and writes the expected header.
check "$WGET_DUMP $EXAMPLE_ROOT/combine_css.html | grep -q X-Mod-Pagespeed"

echo TEST: 404s are served and properly recorded.
NUM_404=$($WGET_DUMP $STATISTICS_URL | grep resource_404_count | cut -d: -f2)
NUM_404=$(($NUM_404+1))
check "wget -O /dev/null $BAD_RESOURCE_URL 2>&1| grep -q '404 Not Found'"
check "$WGET_DUMP $STATISTICS_URL | grep -q 'resource_404_count: $NUM_404'"

# Individual filter tests

echo TEST: combine_css successfully combines 4 CSS files into 1.
fetch_until $EXAMPLE_ROOT/combine_css.html 'grep -c text/css' 1
check $WGET_PREREQ $EXAMPLE_ROOT/outline_css.html
rm -rf $OUTDIR

echo TEST: outline_css outlines large styles, but not small ones.
FILE=outline_css.html
check $WGET_PREREQ $EXAMPLE_ROOT/$FILE
check egrep -q "'<link.*text/css.*large'" $OUTDIR/$FILE  # outlined
check egrep -q "'<style.*small'" $OUTDIR/$FILE           # not outlined
rm -rf $OUTDIR

echo TEST: outline_javascript outlines large scripts, but not small ones.
FILE=outline_javascript.html
check $WGET_PREREQ $EXAMPLE_ROOT/$FILE
check egrep -q "'<script.*src=.*large'" $OUTDIR/$FILE       # outlined
check egrep -q "'<script.*small.*var hello'" $OUTDIR/$FILE  # not outlined
rm -rf $OUTDIR

