# -f tap-status.awk

{
  print
}

END {
  printf "\n# %s\n", status_summary()
}
