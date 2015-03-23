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

You can quickly learn how to use keep-cool on your Mac by following
this simple video tutorial: http://youtu.be/SLeW_hbDjNQ

Anyway, you dnon't need to know all the options if you run the installer script.

```bash
keep-cool [options]
  -a <alg>   : selects fan speed computing alghoritm: (default is quadratic)
      s    -> linear: speed increments constantly between t-min and t-max,
                      this is the _Simple approach
      c    -> logarithmic: speed increments quickly for lower temperatures
                      but slowly for high temperatures, this is the
                      _Conservative (or noisy) approach
      q    -> quadratic: speed increments slowly for lower temperatures
                      but quickly for high temperatures, this is the
                      _Quiet or (noise-reduction) approach
      b    -> cubic: speed increments quickly for lower temperatures, slowly
                      for mid-range temperatures and fastly for high 
                      temperatures, this is a _Balanced compromise between
                      the quiet and conservative approaches
      i    -> i-cubic: speed increments slowly for lower temperatures, quickly
                      for mid-range temperatures and slowly for high 
                      temperatures. This can be seen as an Inverse-Balanced
                      approach, cause it's still a compromise between the
                      quiet and conservative approaches, though it tends to
                      be more quiet, especially for low temperatures.
      w    -> wave: speed increments as a wave: slowly at low temperatures,
                      quickly and then slowly for mid-range temperatures and
                      and finally quickly again for very high temperatures.
                      This is a mathematical experiment that seems to have
                      a nice behavior. It's a "smooth 3-steps" approach with
                      quiet and conservative properties.
  -d         : enable debug mode, dump internal state and values
  -f         : run forever (runs as daemon)
  -g         : generates in the current directory the plist file required to
               run as service using the same arguments passed from command line
  -h         : prints this help
  -l         : dump fan info decoded
  -L         : list all SMC temperature sensors keys and values
  -m <value> : set minimum temperature to start fan throttling (default 60ºC)
  -M <value> : set maximum temperature to set fan max speed (default 92ºC)
  -n         : dry run, do not actually modify fan speed
  -r         : run once and exits
  -s <value> : simulates temperature read as value (for testing purposes)
  -t         : print current temperature
  -T <key>   : uses the provided key as temperature sensor. If you specify '?',
             : then keep-cool will try to guess which is the CPU sensor
  -v         : print version
```

Note: When running as daemon keep-cool log its state in the system logs (syslog).
It accepts 3 unix signals: SIGHUP, SIGUSR1 and SIGUSR2.
These signals switch on the fly the speed computing algorithm:
* SIGHUP -> restores the default algorithm: quadratic
* SIGUSR1 -> selects the next algorithm following the sequence: q,s,c,b,i,w
* SIGUSR2 -> selects the previous algorithm following the sequence: w,i,b,c,s,q

The Selected algorithm is printed on the system.log (/var/log/system.log) and it 
can be seen using the "console" application.

### Release Notes.

####Version 0.6.0 - 03/23/2015
 * Introduced a function that automatically guesses the CPU sensor's name 
 * Improved the installer script, now it suggests the sensor to monitor to the user
 * Improved the uninstaller, now it checks if keep-cool is actually installed

####Version 0.5.0 - 03/22/2015
 * Introduced an installer-wizard and an uninstaller scripts for systems without XCode tools installed
 * Introduced a simple test to prevent inconsistent min and max temperature values

####Version 0.04 - 03/22/2015
 * Changed the temperature polling interval from 1 second to 0.5 seconds in order to make the fan(s) speed transitions smoother.

####Version 0.03 - 03/22/2015
 * Introduced the wave algorithm.
 * Introduced the logging on syslog when running as a service
 * Improved the signals handling

####Version 0.02 - 03/21/2015
 * Introduced the inverse-balanced (inverse-cubic) algorithm.

####Version 0.01 - 04/18/2015
 * First published release.

### Compiling

You don't need to compile it but you can do it if you want.
keep-cool comes already compiled and ready for use.

```bash
make
```

### Installing

Remember to install keep-cool using an Administrator's account. 
The installer needs to write 2 files outside of your home directory.

```bash
sudo ./installer.sh 
```

### Uninstalling
```bash
sudo ./uninstaller.sh
```

### Running

The following command simulates a temperature of 75ºC and lets you see how keep-cool works when using the cubic algorithm.

```bash
./keep-cool -d -n -a b -s 75
```

### Output example

```
Selected Speed Computing Algorithm: cubic (Balanced)
Current temperature: 75.00ºC
Number of Fans: 2
Fan [0]: Min Speed = 0 Current Speed = 2000
Fan [1]: Min Speed = 0 Current Speed = 2004
Computed new fan speed: 4099
```

## Maintainer 

Marco Casavecchia M. (<marcolinuz@gmail.com>)

### Source 

Apple System Management Control (SMC) Tool 
Copyright (C) 2006

### Inspiration 

 * http://www.eidac.de/smcfancontrol/
 * https://github.com/hholtmann/smcFanControl/tree/master/smc-command
