# AI-DJ Coding Instructions

## Project Overview
AI-DJ is a VST plugin and server system for AI-powered music generation and manipulation. The project consists of:

- VST Plugin (`/vst/`) - Audio processing interface built with JUCE framework
- Neural Server (`/server/`) - FastAPI backend for AI model inference 
- Core System (`/core/`) - Python modules managing audio generation and processing

## Key Architecture Patterns

### Component Structure
- **Core Engine** (`/core/dj_system.py`): Central coordinator managing audio layers, generation, and effects
- **Server Interface** (`server_interface.py`): FastAPI server exposing AI capabilities
- **VST Plugin** (`/vst/src/`): JUCE-based audio processor and UI

### Data Flow
1. VST Plugin captures audio/MIDI input
2. Requests sent to Neural Server via REST API
3. Server processes with AI models and returns generated audio
4. VST Plugin renders final output

## Development Workflow

### Environment Setup
```bash
python -m venv obsidian-env
source obsidian-env/bin/activate  # (or activate.bat on Windows)
pip install -r requirements.txt
```

### Building the VST
1. Install CMake and C++ build tools
2. Configure with CMake:
```bash
cd vst
cmake -B build
cmake --build build
```

### Running the Server
```bash
python server_interface.py  # Starts on http://localhost:8000
```

## Key Integration Points

### VST-Server Communication
- REST API defined in `/server/api/routes.py`
- Endpoints for audio generation, effects, and model control
- VST client implementation in `/vst/src/DjIaClient.h`

### Audio Processing Pipeline
1. Input capture (`/vst/src/AudioAnalyzer.h`)
2. AI processing (`/core/music_generator.py`)
3. Effects application (`/vst/src/SimpleEQ.h`)
4. Output rendering (`/vst/src/MasterChannel.h`)

## Project Conventions

### Code Organization
- Core Python logic in `/core/`
- C++ VST code in `/vst/src/`
- Server endpoints in `/server/api/`
- Configuration in `/config/`

### Error Handling
- Use `api_keys_manager.py` for credential management
- Server errors logged via FastAPI middleware
- VST plugin uses JUCE error handling patterns

## Testing
- Audio test files in `/testfiles/`
- Server tests use FastAPI TestClient
- VST plugin tests use JUCE unit testing framework

## Common Operations
- Model download/update: `python -m core.music_generator --update-models`
- Server restart: Kill process and rerun `server_interface.py`
- VST rebuild: Clean build directory and rerun CMake commands
