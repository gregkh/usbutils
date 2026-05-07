BEGIN {
  FS = ":"
}

{
  file = $1
  line = $2
  if (line == "") {
    all[file] = 1
  } else {
    lines[file] = lines[file] " " line
  }
}

END {
  for (file in all) {
    print file "\t"
  }

  for (file in lines) {
    if (!(file in all)) {
      print file "\t" lines[file]
    }
  }
}
