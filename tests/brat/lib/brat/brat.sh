set -eu
TAB="$(printf '\t')"
NL="$(printf '\n ')"
NL="${NL%?}"

alias self='"$SELF"'
alias error='self --- error'
alias usage='self --- usage "$0" "${USAGE:-}"'

${BRAT_DEBUG:+set -x}

brat_cache_fetch() {
  _brat_cache_fetch_path="$(IFS=" "; brat_cache_path "$*")"
  if ! [ -r "$_brat_cache_fetch_path" ]; then
    brat-"$@" >"$_brat_cache_fetch_path.$$" || return 1
    mv "$_brat_cache_fetch_path.$$" "$_brat_cache_fetch_path"
  fi
  printf "%s\n" "$_brat_cache_fetch_path"
}

brat_cache_path() {
  _brat_cache_path_result="$(printf "%s" "$1" | cksum)"
  printf "%s\n" "$BRAT_TMP.cache.${_brat_cache_path_result% *}"
}

brat_is_function() {
  PATH= command -v "$1" >/dev/null 2>&1
}

brat_on_exit() {
  _brat_on_exit="$(printf "%s\n" "$1" "${_brat_on_exit:-}")"
  trap "$_brat_on_exit" EXIT
}

brat_preprocess() {
  brat_cache_fetch test--preprocess "$1"
}
