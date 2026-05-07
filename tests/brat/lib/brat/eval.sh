set -eu

brat_eval_fn() {
  _brat_eval_fn="brat_fn_$1"
  if ! brat_is_function "$_brat_eval_fn"; then
    error "$FILE:$1: not a test definition line"
  fi

  if brat_is_function teardown; then
    brat_on_exit "
      set -eux
      teardown
      { set +x; } 2>/dev/null
    "
  fi

  if brat_is_function setup; then
    set -eux
    setup
    { set +x; } 2>/dev/null
  fi

  set -eu
  "$_brat_eval_fn"
  { set +x; } 2>/dev/null
}

brat_eval_var() {
  eval '[ -n "${'"brat_$1"'+set}" ] && printf "%s" "${'"brat_$1"'#?}"'
}

DIR="${FILE%/*}"

_brat_eval_file="$(brat_preprocess "$FILE")" || exit 1
. "$_brat_eval_file"
