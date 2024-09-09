const addon = require('../build/Release/addon.node');

// Load the background image
const bgImage = new addon.Image();
if (!bgImage.load('build.jpeg')) {
  console.log('Failed to load background image');
  return;
}

// Load the overlay image
const overlayImage = new addon.Image();
if (!overlayImage.load('cat.png')) {
  console.log('Failed to load overlay image');
  return;
}

// Resize the overlay image (e.g., zoom out by 50%)
overlayImage.resize(0.5); // Use a single argument to resize both width and height

// Draw the overlay on the background
const drawSuccess = bgImage.draw(overlayImage, 100, 50);  // Draw at (100, 50)
if (drawSuccess) {
  console.log('Overlay drawn successfully');

  // Save the result
  if (bgImage.save('output.png', 'png')) {
    console.log('Result saved successfully');
  } else {
    console.log('Failed to save result');
  }
} else {
  console.log('Failed to draw overlay');
}
