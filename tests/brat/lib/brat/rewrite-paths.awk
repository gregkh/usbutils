{
  while (i = index($0, source)) {
    $0 = substr($0, 1, i-1) file substr($0, i+length(source))
  }

  while (match($0, /[^ ]*brat-test-run: (line )?[0-9]+:/)) {
    prefix = substr($0, 1, RSTART-1)
    suffix = substr($0, RSTART+RLENGTH)
    matched = substr($0, RSTART, RLENGTH)
    match(matched, /[0-9]+/)
    lineno = substr(matched, RSTART, RLENGTH)
    $0 = prefix file ": line " lineno ":" suffix
  }

  while (match($0, /[^ ]*brat-test-run:/)) {
    $0 = substr($0, 1, RSTART-1) file ":" substr($0, RSTART+RLENGTH)
  }

  while (match($0, test_tmp "[0-9]+\\.[0-9]+\\.out")) {
    $0 = substr($0, 1, RSTART-1) "$stdout" substr($0, RSTART+RLENGTH)
  }

  while (match($0, test_tmp "[0-9]+\\.[0-9]+\\.err")) {
    $0 = substr($0, 1, RSTART-1) "$stderr" substr($0, RSTART+RLENGTH)
  }

  print
}
