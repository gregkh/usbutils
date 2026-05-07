set -eu

alias _xtrace_silence='{ _xtrace=0; case $- in *x* ) _xtrace=1; esac; set +x; } 2>/dev/null'
alias _xtrace_restore='{ [ $_xtrace -eq 0 ] || set -x; } 2>/dev/null'

TEST_TMP="$BRAT_TMP.test.$$"
brat_on_exit '{ rm -fr "$TEST_TMP"*; } 2>/dev/null'

run() {
  _xtrace_silence

  _run_count=${_run_count:-0}
  _run_count=$((_run_count + 1))
  status=0
  stdout="$TEST_TMP.$_run_count.out"
  stderr="$TEST_TMP.$_run_count.err"

  "$@" >"$stdout" 2>"$stderr" || status=$?

  if [ $status -eq 127 ]; then
    { cat "$stderr"
      _xtrace_restore
      return $status
    } 2>/dev/null
  fi

  _xtrace_restore
}

match() {
  _xtrace_silence

  _match_status=0
  MATCH_PATTERN="$2" awk -f "${SELF%/*/*}/lib/brat/match.awk" "$1" || _match_status=$?

  { _xtrace_restore
    return $_match_status
  } 2>/dev/null
}

compare() {
  _xtrace_silence

  _compare_status=0
  [ -r "$1" ] && [ -r "$2" ] && [ "$(cksum <"$1")" = "$(cksum <"$2")" ] || _compare_status=$?

  { _xtrace_restore
    return $_compare_status
  } 2>/dev/null
}
