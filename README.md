# MetricBench

A windowed time-series benchmark focused on capturing metrics from devices, processing and purging expired data.

## Building

### Prerequisites:

* C++ compiler supporting c++11 (g++-4.8+, Clang 3.3+)
* CMake / make
* MySQL C Client headers and library
* MySQL Connector C++ headers and library (static)
* Boost system, filesystem and program options headers and libraries

### Build:

```bash
$ cd src
$ cmake .
$ make
```

## Running

### Prerequisites: 

* MySQL server

### Command line options:

```
  --help                            Help message
  --mode arg                        Mode - run or prepare (load initial 
                                    dataset)
  --url arg (=tcp://127.0.0.1:3306) Connection URL, e.g. tcp://[host[:port]], 
                                    unix://path/socket_file 
  --database arg (=test)            Connection Database (schema) to use
  --user arg (=root)                Connection User for login
  --password arg                    Connection Password for login
  --days arg (=10)                  Days of traffic to simulate
  --threads arg (=8)                Working threads
  --engine arg (=InnoDB)            Set storage engine
  --engine-extra arg                Extra storage engine options, e.g. 
                                    'ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8'
```

### Example usage:

First the metric table needs to be created and loaded with initial data.

```bash
$ ./MetricBench --mode=prepare
```

Secondly, the run phase simulates windowed time-series operation on the database.

```bash
$ ./MetricBench --mode=run
```


