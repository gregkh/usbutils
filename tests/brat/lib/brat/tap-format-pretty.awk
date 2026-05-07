# -f terminal.awk -f wcwidth.awk -f tap-status.awk

BEGIN {
  cur = 0
  max = 0
  tag = ""
  bold = terminal_seq("0;1m")
  red = terminal_seq("31;1m")
  reset = terminal_seq("0m")
}

/^TAP version/ {
  print
  next
}

/^1\.\./ {
  print
}

/^$/ && cur == 0 {
  print
}

/^(ok|not ok|#)/ {
  line = terminal_strip($0)
}

/^(ok|not ok) [0-9]+/ {
  match($0, /[0-9]+/)
  cur = int(substr($0, RSTART, RLENGTH))
  max = (cur > max) ? cur : max
  tag = $1
}

/^(ok|not ok)/ {
  if (tag == "not") {
    line = \
      terminal_seq("31;1m") substr(line, 1, 6) \
      terminal_seq("0;1m") substr(line, 7)
  }

  changes[cur] = line "\n"
  draw()
}

/^#/ && cur > 0 {
  if (!(cur in changes)) {
    changes[cur] = lines[cur]
  }

  if (tag == "not") {
    line = terminal_seq("31m") "#" terminal_seq("0m") substr(line, 2)
  } else {
    line = terminal_seq("2m") "#" terminal_seq("0m") substr(line, 2)
  }

  changes[cur] = changes[cur] line "\n"
}

END {
  draw()
  printf "\n%s# %s%s%s\n", terminal_seq("2m"), terminal_seq("0m"), status_summary(bold, red, reset), terminal_seq("K")
}

function prepare(__, i, head, result) {
  result = 0

  for (i = 1; i <= max; i++) {
    if (i in changes) {
      result = result ? result : i
    } else if (!(i in lines)) {
      changes[i] = terminal_seq("2m") "#  " terminal_seq("0m") i "\n"
      result = result ? result : i
    }
  }

  return result
}

function draw(__, head, height, i, out, width) {
  head = prepare()
  if (!head) return

  height = 0
  width = terminal_width()
  out = ""

  for (i = head; i <= max; i++) {
    if (lines[i]) {
      height = height + measure_height(lines[i], width)
    }

    if (i in changes) {
      lines[i] = changes[i]
      delete changes[i]
    }

    out = out lines[i]
  }

  if (height) {
    out = terminal_seq(height "A") out
  }

  out = out "\n\n" \
    terminal_seq("1A") \
    terminal_seq("2m") "# " terminal_seq("0m") status_summary(bold, red, reset) "\n" \
    terminal_seq("2A")

  gsub(/\n/, terminal_seq("K") "&" terminal_seq("0m"), out)
  printf "%s", out
  fflush()
}

function measure_height(str, width, __, line, lines, count, height, i, result) {
  if (width != last_width) {
    last_width = width
    split("", measurements)
  }

  sub(/\n$/, "", str)
  count = split(str, lines, /\n/)
  result = 0

  for (i = 1; i <= count; i++) {
    line = lines[i]
    if (line in measurements) {
      height = measurements[line]
    } else {
      height = int(wcscolumns(lines[i]) / width) + 1
      measurements[line] = height
    }

    result = result + height
  }

  return result
}
