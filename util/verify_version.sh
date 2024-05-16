#!/bin/sh

# Check that all files that should have the current version agree on it.
# Used to fail RPM builds from sw that is not properly updated before tagging.

# ----------------------------------------------------------------------------
# Assume success
# ----------------------------------------------------------------------------

EXIT_CODE=0

printf >&2 "Checking package versioning...\n"

# ----------------------------------------------------------------------------
# The "master" version = whatever is in Makefile
# ----------------------------------------------------------------------------

RESULT="req"
MASTER_PATH=Makefile
MASTER_VERS=$(grep '^VERSION.*=' $MASTER_PATH |sed -e 's/^.*=[[:space:]]*//')
printf >&2 "  %-32s %-3s %s\n" "$MASTER_PATH" "$RESULT" "$MASTER_VERS"

# ----------------------------------------------------------------------------
# Check rpm spec file version, with relaxed rules
# - "1.2.3" == "1.2.3.4"
# - "1.2.3" == "1.2.3+patchlevel"
# ----------------------------------------------------------------------------

RESULT="ack"
OTHER_PATH=${RPM_SOURCE_DIR:-rpm}/${RPM_PACKAGE_NAME:-sensors-glib}.spec
if [ -f "$OTHER_PATH" ]; then
  OTHER_VERS=$(grep '^Version:' $OTHER_PATH |sed -e 's/^.*:[[:space:]]*//')
else
  OTHER_VERS="n/a"
fi
EXTRA_VERS=${OTHER_VERS#$MASTER_VERS}
if [ "$EXTRA_VERS" != "" ]; then
  if [ "$MASTER_VERS$EXTRA_VERS" != "$OTHER_VERS" ]; then
    RESULT="NAK"
    EXIT_CODE=1
  fi
fi
printf >&2 "  %-32s %-3s %s\n" "$OTHER_PATH" "$RESULT" "$OTHER_VERS"

# ----------------------------------------------------------------------------
# In case of conflicts, exit with error value
# ----------------------------------------------------------------------------

if [ $EXIT_CODE != 0 ]; then
  printf >&2 "\nFatal: Conflicting package versions\n"
fi

exit $EXIT_CODE
