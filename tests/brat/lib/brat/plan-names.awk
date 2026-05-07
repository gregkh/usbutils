BEGIN {
  FS = "\t"
  include_count = split(inc, includes, "\t")
  exclude_count = split(exc, excludes, "\t")
}

function matches(str, pattern, __, len) {
  len = length(pattern)
  if (len == 0) {
    return 0
  } else if (substr(pattern, 1, 1) == "/" && substr(pattern, len, 1) == "/") {
    return str ~ substr(pattern, 2, len - 2)
  } else {
    return str == pattern
  }
}

{
  name = $4

  for (i = 1; i <= exclude_count; i++) {
    if (matches(name, excludes[i])) {
      next
    }
  }

  if (include_count > 0) {
    matched = 0
    for (i = 1; i <= include_count; i++) {
      if (matches(name, includes[i])) {
        matched = 1
        break
      }
    }
    if (!matched) {
      next
    }
  }

  print
}
