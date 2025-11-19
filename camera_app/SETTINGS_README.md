# Settings Configuration

This application uses a JSON file (`settings.json`) to store and load configuration parameters.

## File Location

The `settings.json` file should be located in the same directory as the executable.

## Structure

```json
{
    "frangi": {
        "sigma": 1.5,                      // Gaussian blur sigma (0.1 - 5.0)
        "beta": 0.5,                       // Plate sensitivity (0.1 - 5.0)
        "c": 15.0,                         // Contrast parameter (0.1 - 50.0)
        "displayStage": 8,                 // Which stage to display (0-8)
        "invertEnabled": true,             // Invert for dark structures
        "segmentationThreshold": 0.01      // Binary threshold (0.001 - 0.1)
    },
    "preprocessing": {
        "globalContrast": {
            "enabled": false,              // Enable global contrast adjustment
            "brightness": 20.0,            // Brightness offset (0 - 100)
            "contrast": 3.0                // Contrast multiplier (0 - 10)
        },
        "clahe": {
            "enabled": false,              // Enable CLAHE enhancement
            "maxIterations": 2,            // Max iterations (1 - 5)
            "targetContrast": 0.3          // Target contrast (0 - 0.9)
        }
    },
    "camera": {
        "selectedIndex": 0                 // Index in available cameras list
    }
}
```

## Display Stages

- **0**: Grayscale - Input converted to grayscale
- **1**: Invert - Inverted grayscale (for dark structures)
- **2**: Blur - Gaussian blurred image
- **3**: Gradients - Sobel gradients (magnitude)
- **4**: Hessian - Second derivatives matrix
- **5**: Eigenvalues - Sorted eigenvalues
- **6**: Vesselness - Frangi filter probability map
- **7**: Segmentation - Binary mask (thresholded)
- **8**: Overlay - Binary mask overlaid on original (default)

## Usage

### In Application

1. **Auto-load**: Settings are automatically loaded at startup
2. **Manual Save**: Click "Save Settings" button in GUI to save current parameters
3. **Manual Load**: Click "Load Settings" button in GUI to reload from file
4. **Reset to Defaults**: Click "Reset to Defaults" button to restore default values

**Note**: Settings are NOT automatically saved on exit. You must manually save them using the "Save Settings" button.

### Manual Editing

You can edit `settings.json` with any text editor while the application is not running.
The file will be validated on load, and any missing parameters will use default values.

## Default Values

If the file doesn't exist, it will be automatically created with default values on first run.

