BEGIN {
  directives["skip"] = 1
  directives["test"] = 1
  directives["todo"] = 1
  prefix="brat_"
}

/^[ \t]*@[^ \t]+[ \t]+.*\{[ \t]*(#.*)?$/ {
  match($0, /@[^ \t]+/)
  name_start = RSTART + 1
  name_end = RSTART + RLENGTH
  name = substr($0, name_start, name_end - name_start)

  match($0, /\{[ \t]*(#.*)?$/)
  brace_start = RSTART

  arg = substr($0, name_end, brace_start - name_end)
  sub(/^[ \t]*/, "", arg)
  sub(/[ \t]*$/, "", arg)

  if (name in directives) {
    if (arg == "") {
      error(name, "missing argument")
    }
  } else {
    error(name, "unknown directive")
  }

  id = prefix name "_" NR
  fn = prefix "fn_" NR
  ws = substr($0, 1, name_start - 2)

  index_expr = "\"${" prefix name ":-} " NR "\""
  assign_expr = id "=:" arg " " prefix name "=" index_expr
  init_expr = "set -x"

  print ws assign_expr "; " fn "() { " init_expr
  next
}

{
  print
}

function error(name, message) {
  print "error: line " NR ": @" name ": " message | "cat 1>&2"
  exit 2
}
