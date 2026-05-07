BEGIN {
  status["fail"] = 0
  status["pass"] = 0
  status["run"] = 0
  status["skip"] = 0
  status["todo"] = 0
  status["total"] = 0
}

/^1\.\./ {
  status["total"] = int(substr($0, 4))
}

/^(ok|not ok)/ {
  if (status["total"] == 0) {
    exit 1
  }

  directive = find_directive($0)
  if (directive) {
    status[directive]++
  }

  status["run"]++
}

/^ok / && (directive == "" || directive == "todo") {
  status["pass"]++
}

/^not ok / && directive != "todo" {
  status["fail"]++
}

function find_directive(str, __, tail) {
  tail = str
  while (match(tail, / # *(SKIP|TODO)/)) {
    if (RSTART > 1 && substr(tail, RSTART - 1, 1) == "\\") {
      tail = substr(tail, RSTART + 2)
      continue
    }
    match(tail, / # *(SKIP|TODO)/)
    return tolower(substr(tail, RSTART + RLENGTH - 4, 4))
  }
}

function status_summary(bold, red, reset, __, detail, run, success, symbol, total) {
  run = status["run"]
  success = status["fail"] == 0
  total = status["total"]

  if (total == 1) {
    s = ""
  } else {
    s = "s"
  }

  if (success) {
    if (run == total) {
      symbol = bold "✓"
    }
  } else {
    symbol = red "✘"
    if (run != total) {
      symbol = " " symbol
    }
  }

  detail = sprintf("(%d passed, %d failed, %d skipped)", status["pass"], status["fail"], status["skip"])

  if (run == total) {
    return sprintf(" %s %s%d test%s%s %s", symbol, bold, total, s, reset, detail)
  } else {
    return sprintf("%s%s %d/%d test%s %s", symbol, reset, run, total, s, detail)
  }
}
