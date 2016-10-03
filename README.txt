# This is the readme for pman

pman is a lightweight process manager capable of manipulating processes using the Linux system-level API. Pman is written in C.

Commands:
- bg <command>: start a process on a new process (Usage Example: bg cat foo.txt)
- bglist: list all of the background processes started by pman. (Usage Example: bglist)
- bgkill <pid>: kill any process based on it's assigned pid. (Usage Example: bgkill 1337)
- bgstop <pid>: stop any process based on it's assigned pid. (Usage Example: bgstop 1337)
- bgstart <pid>: start any previously stopped process based on it's assigned pid. (Usage Example: bgstart 1337)
- pstat <pid>: Display the stats information for a process based on it's assigned pid. (Usage Example: pstat 1337)

