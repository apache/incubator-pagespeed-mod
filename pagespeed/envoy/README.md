# Envoy filter example

This project implements `PageSpeed` as an `Envoy`-filter.

## Building

To build the Envoy static binary:

1. `bazel build //pagespeed/envoy:envoy`

## Testing

To run the pagespeed envoy filter integration test:

`bazel test //pagespeed/envoy:http_filter_integration_test`


## Configuration


```yaml
http_filters:
- name: pagespeed          # before envoy.router because order matters!
  config:
    key: via
    val: pagespeed-filter
- name: envoy.router
  config: {}
```
