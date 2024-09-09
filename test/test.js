const { test } = require('node:test');
const assert = require('assert');
const fs = require('fs');
const Image = require('../build/Release/addon.node').Image; // Adjust the path and module name accordingly

test('Create an empty image with default dimensions', () => {
  const defaultImage = new Image();
  assert.strictEqual(defaultImage.getWidth(), 0);
  assert.strictEqual(defaultImage.getHeight(), 0);
});

test('Create a square image with one argument', () => {
  const size = 150;
  const squareImage = new Image(size);
  assert.strictEqual(squareImage.getWidth(), size);
  assert.strictEqual(squareImage.getHeight(), size);
});

test('Create an empty image with specified width and height', () => {
  const width = 200;
  const height = 100;
  const emptyImage = new Image(width, height);
  assert.strictEqual(emptyImage.getWidth(), width);
  assert.strictEqual(emptyImage.getHeight(), height);
});

test('Resize the image (zoom out by 50%)', () => {
  const img = new Image(200, 200);
  img.resize(0.5);
  assert.strictEqual(img.getWidth(), 100);
  assert.strictEqual(img.getHeight(), 100);
});

test('Resize the image (zoom in by 200%)', () => {
  const img = new Image(100, 100);
  img.resize(2.0, 2.0);
  assert.strictEqual(img.getWidth(), 200);
  assert.strictEqual(img.getHeight(), 200);
});

test('Save the image as PNG', () => {
  const img = new Image(100, 100);
  const filePath = 'test_output.png';
  
  // Ensure the file does not exist before the test
  if (fs.existsSync(filePath)) {
    fs.unlinkSync(filePath);
  }

  // Save the image
  const saveResult = img.save(filePath, 'png');
  assert.strictEqual(saveResult, true);

  // Check if the file was created
  assert.strictEqual(fs.existsSync(filePath), true);

  // Clean up the test file
  fs.unlinkSync(filePath);
});

test('Save the image as JPEG', () => {
  const img = new Image(100, 100);
  const filePath = 'test_output.jpg';
  
  // Ensure the file does not exist before the test
  if (fs.existsSync(filePath)) {
    fs.unlinkSync(filePath);
  }

  // Save the image
  const saveResult = img.save(filePath, 'jpeg');
  assert.strictEqual(saveResult, true);

  // Check if the file was created
  assert.strictEqual(fs.existsSync(filePath), true);

  // Clean up the test file
  fs.unlinkSync(filePath);
});

test('Draw an overlay image onto a background image', () => {
  const bgImage = new Image(300, 300);
  const overlayImage = new Image(100, 100);

  // Resize the overlay image (e.g., zoom out by 50%)
  overlayImage.resize(0.5);

  // Draw the overlay on the background
  const drawSuccess = bgImage.draw(overlayImage, 100, 100);  // Draw at (100, 100)
  assert.strictEqual(drawSuccess, true);

  const filePath = 'test_draw_output.png';
  
  // Ensure the file does not exist before the test
  if (fs.existsSync(filePath)) {
    fs.unlinkSync(filePath);
  }

  // Save the resulting image
  const saveResult = bgImage.save(filePath, 'png');
  assert.strictEqual(saveResult, true);

  // Check if the file was created
  assert.strictEqual(fs.existsSync(filePath), true);

  // Clean up the test file
  fs.unlinkSync(filePath);
});
