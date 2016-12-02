config_title="<title>PageSpeed Configuration</title>"

if [ "$NO_VHOST_MERGE" = "on" ]; then
  start_test Config with VHost inheritance off
  echo $WGET_DUMP $SECONDARY_CONFIG_URL
  SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
  check_from "$SECONDARY_CONFIG" fgrep -q "$config_title"
  # No inherit, no sharding.
  check_not_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

  # Should not inherit the blocking rewrite key.
  check_not_from "$SECONDARY_CONFIG" egrep -q "blrw"
else
  start_test vhost inheritance works
  echo $WGET_DUMP $SECONDARY_CONFIG_URL
  SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
  check_from "$SECONDARY_CONFIG" fgrep -q "$config_title"
  # Sharding is applied in this host, thanks to global inherit flag.
  check_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

  # We should also inherit the blocking rewrite key.
  check_from "$SECONDARY_CONFIG" egrep -q "\(blrw\)[[:space:]]+psatest"
fi
