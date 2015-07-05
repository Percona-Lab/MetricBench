# MetricBench

A windowed time-series benchmark focused on capturing metrics from devices, processing and purging expired data.

## Building

### Prerequisites:

* C++ compiler supporting c++11 (g++-4.8+, Clang 3.3+)
* CMake / make
* MySQL C Client headers and library
* MySQL Connector C++ headers and library (static)
* Boost system, filesystem , program options and unit testing headers and libraries

### Build:

```bash
$ cd src
$ cmake .
$ make
```

## Schema
The current implementation proposes a following schema

```
CREATE TABLE metricsN
    	ts timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
    	org_id int(10) unsigned NOT NULL,
    	device_id int(10) unsigned NOT NULL,
    	metric_id int(10) unsigned NOT NULL,
    	cnt int(10) unsigned NOT NULL,
    	val double DEFAULT NULL,
    	PRIMARY KEY (ts, org_id, device_id, metric_id),
    	KEY k1 (org_id, device_id, metric_id, ts, val),
    	KEY k2 (org_id, metric_id, ts, val),
    	KEY k3 (metric_id, ts, org_id, device_id ,val)
)
```

Where we create N similar tables (N=10 by default), and `org_id` is in the range 1..10, `device_id` is in the range 1..30 and `metric_id` is in range 1..300
		    
		    

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


