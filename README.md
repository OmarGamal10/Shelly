# Shelly

Shelly is a simple UNXI shell implementation written in C. It provides core shell functionality along with piping, redirection and command history navigation via arrows.

## Features

Shelly currently supports:

- Basic UNIX command execution
	
- Interactive mode and non-interactive (script) mode
    
- Passing cli arguments with support for spaces and quotes
    
- Built-in commands:
    
    - cd - Change directory (navigates to HOME if no arguments specified)
        
    - pwd - Print working directory
        
    - path - Set search paths for command executable discovery (default set is usr/bin)
        
    - exit - Exit the shell (works the same as CTRL+D)
        
- I/O redirection with the > operator
    
- Multi pipe support
    
- Arrow navigation per command
	 
- Command history navigation (via ARROWUP and ARROWDN)
	
- Globbing (wildcard matching) support (aka: `*.txt` spreads to all text files in a directory
	
- Signal handling (CTRL+C sends a SIGINT foreground child processes . So `ping google.com` is killed, while the shell process itself and background processes are not affected)
## Building and Usage

Building is as simple as cloning the repo, downloading the readline library with GCC and running the exectubable
#### Ubuntu or WSL 
```
sudo apt update
sudo apt install libreadline-dev
```

```bash
git clone https://github.com/OmarGamal10/Shelly.git
cd shelly
make
./shelly
```

 License

This project is licensed under the MIT License - see the LICENSE file for details.