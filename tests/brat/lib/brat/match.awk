BEGIN {
  status = 1

  if (!("MATCH_PATTERN" in ENVIRON)) {
    fail()
  }

  mode = "exact"
  opts = ""
  pattern = ENVIRON["MATCH_PATTERN"]

  if (match(pattern, /^\/.*\/i?$/)) {
    mode = "regex"
    opts = pattern
    sub(/^.*\//, "", opts)

    body = substr(pattern, 2, length(pattern) - length(opts) - 2)

    if (opts ~ /i/) {
      body = tolower(body)
    }
  }
}

opts ~ /i/ {
  $0 = tolower($0)
}

mode == "regex" && match($0, body) {
  pass()
}

mode == "exact" && index($0, pattern) {
  pass()
}

END {
  exit status
}

function fail() {
  exit
}

function pass() {
  status = 0
  exit
}
