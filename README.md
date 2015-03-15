# Keep-Cool

Handles SMC Min Fan Speed parameter in order to dynamically
modulate fans speed wiht CPU temperature for OSX

The "Algorithms.png" picture explains the behavior of the 
implemented computing algorithms.
Feel free to test all of them and use the one you prefer.

Note: This program is meant to run as service, the install
operation copies the plist file and enables keep-cool to 
run as a service at system startup.

## Usage 

```bash
keep-cool [options]
  -a <alg>   : selects fan speed computing alghoritm: (default is cubic)
      e    -> linear: speed increments constantly between t-min and t-max,
                      this is the _Easy (or simple) approach
      c    -> logarithmic: speed increments quickly for lower temperatures
                      but slowly for high temperatures, this is the
                      _Conservative (or noisy) approach
      s    -> quadratic: speed increments slowly for lower temperatures
                      but quickly for high temperatures, this is the
                      _Silent or (noise-reduction) approach
      b    -> cubic: speed increments quickly for lower temperatures, slowly
                      for mid-range temperatures and fastly for high 
                      temperatures, this is a _Balanced compromise between
                      the silent and conservative approaches
  -d         : enable debug mode, dump internal state and values
  -f         : run forever (runs as daemon)
  -h         : prints this help
  -l         : dump fan info decoded
  -L         : list all SMC temperature sensors keys and values
  -m <value> : set minimum temperature to start fan throttling (default 60ºC)
  -M <value> : set maximum temperature to set fan max speed (default 92ºC)
  -n         : dry run, do not actually modify fan speed
  -r         : run once and exits
  -s <value> : simulates temperature read as value (for testing purposes)
  -t         : print current temperature
  -T <key>   : uses the key as sensor for the temperature (default "TCXC")
  -v         : print version
```

### Compiling
```bash
make
```

### Installing
```bash
sudo make install #Installs on /usr/local/sbin
```

### Uninstalling
```bash
sudo make uninstall
```

### Running

```bash
./keep-cool -r
```

### Output example

```
Temperature 61.8°C
```

## Maintainer 

Marco Casavecchia M.<marcolinuz@gmail.com>

### Source 

Apple System Management Control (SMC) Tool 
Copyright (C) 2006

### Inspiration 

 * http://www.eidac.de/smcfancontrol/
 * https://github.com/hholtmann/smcFanControl/tree/master/smc-command
