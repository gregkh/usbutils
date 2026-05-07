cmd= skip=

for arg; do
  if [ -n "$skip" ]; then
    skip=
    continue
  fi

  case "$arg" in
    -j | --jobs | -n | --name | -e | --exclude )
      skip=1
      ;;
    *.brat | *.brat:* )
      cmd="brat-plan-run"
      break
      ;;
  esac
done

if [ -n "$cmd" ]; then
  exec "$cmd" "$@"
fi
