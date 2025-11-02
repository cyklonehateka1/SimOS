# SimOS Project Structure

```
SimOS/
│
├── common/
│   └── utils/
│       └── logging.c          # Common logging utilities
│
├── controller/
│   ├── cli.c                  # CLI implementation
│   └── main.c                 # Controller main entry point
│
├── core/
│   ├── config/
│   │   └── env.c              # Environment configuration
│   ├── ipc/
│   │   └── init.c             # IPC initialization
│   └── loop.c                 # Core event loop
│
├── examples/                  # Example files (empty)
│
├── include/
│   ├── cli.h                  # CLI header
│   ├── controller.h           # Controller header
│   ├── env.h                  # Environment header
│   ├── init.h                 # IPC init header
│   ├── logging.h             # Logging header
│   └── loop.h                # Loop header
│
├── node/
│   └── main.c                 # Node main entry point
│
├── config.yaml               # Configuration file
├── Makefile                  # Build configuration
└── README.md                 # Project documentation
```

## Directory Organization

### `/common`

Contains shared utilities and common code used across the project.

### `/controller`

Controller module with CLI interface and main controller logic.

### `/core`

Core system components including:

- **config/**: Environment and configuration management
- **ipc/**: Inter-process communication initialization
- **loop.c**: Main event loop implementation

### `/include`

Header files for all modules, providing the public API.

### `/node`

Node-specific implementation and entry point.

### Root Files

- `config.yaml`: Project configuration
- `Makefile`: Build system configuration
- `README.md`: Project documentation
