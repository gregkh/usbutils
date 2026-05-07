${BRAT_DEBUG:+set -x}

USAGE="$USAGE | [-j <JOBS>] [-n <PATTERN>]... [-e <PATTERN>]... <FILE>[:<LINE>]..."
VERSION="0.10.1"

TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
export TMPDIR

export BRAT_LIB="$ROOT/lib/$NAME"

if [ -z "${BRAT_RUN+set}" ]; then
  export BRAT_RUN="$$"
  export BRAT_TMP="$TMPDIR/$NAME.$BRAT_RUN"

  trap 'rm -fr "$BRAT_TMP"* 2>/dev/null' EXIT
  "$SELF" "$@"
  exit
fi
