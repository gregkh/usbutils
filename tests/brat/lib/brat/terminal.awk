function terminal_seq(seq) {
  return "\033[" seq
}

function terminal_strip(str) {
  gsub(/\033\[[^A-Za-z]*[A-Za-z]/, "", str)
  return str
}

function terminal_width() {
  "tput cols" | getline
  close("tput cols")
  return int($1)
}
