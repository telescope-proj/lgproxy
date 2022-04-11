# Telescope Looking Glass Proxy
[![CodeQL](https://github.com/telescope-proj/lgproxy/actions/workflows/codeql-analysis.yaml/badge.svg)](https://github.com/telescope-proj/lgproxy/actions/workflows/codeql-analysis.yaml)

## Dependencies
### Fedora/RHEL/Rocky
```
  sudo dnf install -y libfabric libfabric-devel protobuf-c-compiler protobuf-c-devel
```
### Ubuntu
```
  sudo apt install -y protobuf-c-compiler libprotobuf-c-dev
```
* Latest version of [Libfabric](https://github.com/ofiwg/libfabric) (Note: If you are running Ubuntu you need to compile from source, the version in the repository is too old.)

## Building the project
```
  git clone https://github.com/telescope-proj/lgproxy.git
  cd lgproxy/lgproxy
  cmake . && make -j $(nproc)
```

## Running Source
```
LGProxy Source Usage Guide
 -h     Address to allow connections from (set to '0.0.0.0' to allow from all)
 -p     Port to listen to for incoming NCP connections
 -f     SHM file to read data from Looking Glass Host
 -s     Size to allocate for SHM File in Bytes
```

## Running Sink
```
LGProxy Sink Usage Guide
 -h     Host to connect to
 -p     Port to connect to on the host for the NCP Channel
 -f     SHM file to write data into
 -s     Size to allocate for SHM File in Bytes
```

## Set Log Level
`LP_LOG_LEVEL=<value>`
`TRF_LOG_LEVEL=<value>`
* 1 = Trace
* 2 = Debug
* 3 = Info
* 4 = Warn
* 5 = Error
* 6 = Fatal (Default value)