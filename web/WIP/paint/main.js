// Display drawing front-end design for P4Matrix64x32Fun project.

function drawBresenhamLine(ctx, x0, y0, x1, y1) {
	if (x0 == x1) {
		return ctx.fillRect(x0, Math.min(y0, y1), 1, Math.abs(y1 - y0) + 1);
	}
	if (y0 == y1) {
		return ctx.fillRect(Math.min(x0, x1), y0, Math.abs(x1 - x0) + 1, 1);
	}
	if (x0 > x1) {
		[x0, x1] = [x1, x0];
		[y0, y1] = [y1, y0];
	}
	
	const dx = x1 - x0;
	const dy = Math.abs(y1 - y0);
	const sy = y1 > y0 ? +1 : -1;
	let err = dx - dy;
	while (true) {
		ctx.fillRect(x0, y0, 1, 1);
		// console.log('line: ', x0, y0, x1, y1, err);
		// if (x0 === undefined || y0 === undefined) throw 'wtf';
		// if (x0 > 100 || y0 > 100 || y0 < 0) throw 'wtf2';
		if (x0 == x1 && y0 == y1) {
			break;
		}

		const e2 = err * 2;
		if (-e2 <= dy) {
			err -= dy;
			x0 += 1;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

// Util to download file 
function downloadBlob(blob, fileName) {
	const url = window.URL.createObjectURL(blob);
	const a = document.createElement('a');
	a.style.display = 'none';
	document.body.appendChild(a);
	a.href = url;
	a.download = fileName;
	a.click();
	window.URL.revokeObjectURL(url);
	a.remove();
}

const displayCanvas = document.querySelector('#display canvas');
displayCanvas.width = 64;
displayCanvas.height = 32;

const ctx = displayCanvas.getContext('2d', { alpha: false, willReadFrequently: true });
ctx.imageSmoothingEnabled = false;

ctx.fillStyle = 'rgb(200, 0, 0)'; 
ctx.fillRect(0, 0, 1, 1);
drawBresenhamLine(ctx, 3, 4, 8, 9);

const fillingModeCheckbox = document.querySelector('input[name=filling]');

const primaryColorPicker = document.querySelector('input[type=color][name=primary]');
const secondaryColorPicker = document.querySelector('input[type=color][name=secondary]');

const undoButton = document.querySelector('button[name=undo]');
const redoButton = document.querySelector('button[name=redo]');

class History {
	constructor(initialState) {
		this.stack = [initialState];
		this.position = 0;
	}
	
	push(state) {
		if (this.canRedo()) {
			this.stack.length = this.position + 1;
		}
		this.stack.push(state);
		this.position++;
		while (this.position > 400) {
			this.stack.shift();
			this.position--;
		}
	}
	canRedo() {
		return this.position < (this.stack.length - 1);
	}
	redo() {
		if (!this.canRedo()) throw new Error('nothing more to redo');
		return this.stack[++this.position];
	}
	canUndo() {
		return 0 < this.position;
	}
	undo() {
		if (!this.canUndo()) throw new Error('nothing more to undo');
		return this.stack[--this.position];
	}
	getCurrent() {
		return this.stack[this.position];
	}
}

const drawingState = {
	tool: 0, // 0 - none, 1 - pixel, 2 - line, 3 - rectangle, 16 - color picker
	pressed: false,
	dragging: false,
	history: new History({
		description: 'initial',
		fullImageData: ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height),
	}),
}

displayCanvas.addEventListener('mousedown', function(e) {
	if (drawingState.tool == 0) return;
	const x = Math.round(e.offsetX / this.clientWidth * this.width - 0.5);
	const y = Math.round(e.offsetY / this.clientHeight * this.height - 0.5);
	if (drawingState.tool == 16) {
		const rgb = Array.prototype.slice.call(ctx.getImageData(x, y, 1, 1).data, 0, 3);
		const hex = '#' + rgb.map(x => x.toString(16).padStart(2, 0)).join('');
		switch (event.button) {
			case 0:
				primaryColorPicker.value = hex;
				break;
			case 2:
				secondaryColorPicker.value = hex;
				break;
			default:
				return; // no drawing
		}
	}
	switch (event.button) {
		case 0:
			ctx.fillStyle = primaryColorPicker.value;
			break;
		case 2:
			ctx.fillStyle = secondaryColorPicker.value;
			break;
		default:
			return; // no drawing
	}
	drawingState.pressed = {x, y};
	if (drawingState.tool == 1) {
		ctx.fillRect(x, y, 1, 1);
	}
	else {
		const state = drawingState.history.getCurrent();
		if (!state.fullImageData) {
			state.fullImageData = ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height);
		}
	}
});
displayCanvas.addEventListener('mousemove', function(e) {
	if (!drawingState.pressed) return;
	if (drawingState.tool == 0 || drawingState.tool == 16) return;
	const x = Math.round(event.offsetX / displayCanvas.clientWidth * displayCanvas.width - 0.5);
	const y = Math.round(event.offsetY / displayCanvas.clientHeight * displayCanvas.height - 0.5);
	
	if (drawingState.dragging && drawingState.dragging.x == x && drawingState.dragging.y == y) return;
	drawingState.dragging = {x, y};
		
	if (drawingState.tool != 1) {
		const state = drawingState.history.getCurrent();
		ctx.putImageData(state.fullImageData, 0, 0);
	}
	
	const x0 = drawingState.pressed.x;
	const y0 = drawingState.pressed.y;
	switch (drawingState.tool) {
		case 1:
			ctx.fillRect(x, y, 1, 1);
			break;
		case 2: // drawing line
			drawBresenhamLine(ctx, x0, y0, x, y);
			break;
		case 3: // drawing rectangle
			if (fillingModeCheckbox.checked) {
				ctx.fillRect(x0 + (x < x0 ? 1 : 0), 
											y0 + (y < y0 ? 1 : 0), 
											x - x0 + (x < x0 ? -1 : 1), 
											y - y0 + (y < y0 ? -1 : 1));
			}
			else {
				const x0f = (x < x0 ? 1 : 0);
				const y0f = (y < y0 ? 1 : 0);
				const x1f = (x < x0 ? -1 : 1);
				const y1f = (y < y0 ? -1 : 1);
				ctx.fillRect(x0 + x0f, y0, x - x0 + x1f, 1);
				ctx.fillRect(x0, y0 + y0f, 1, y - y0 + y1f);
				ctx.fillRect(x0 + x0f, y, x - x0 + x1f, 1);
				ctx.fillRect(x, y0 + y0f, 1, y - y0 + y1f);
			}
			break;
	}
});
displayCanvas.addEventListener('mouseup', function() {
	if (drawingState.tool == 0 || drawingState.tool == 16) return;
	drawingState.history.push({
		description: `draw`,
		fullImageData: ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height),
	});
	undoButton.disabled = false;
	redoButton.disabled = true;
	
	drawingState.pressed = false;
	drawingState.dragging = false;
});
displayCanvas.addEventListener("contextmenu", e => e.preventDefault());

document.querySelector('button[name=view]').addEventListener('click', () => {
	drawingState.tool = 0;
});
document.querySelector('button[name=pixel]').addEventListener('click', () => {
	drawingState.tool = 1;
});
document.querySelector('button[name=line]').addEventListener('click', () => {
	drawingState.tool = 2;
});
document.querySelector('button[name=rectangle]').addEventListener('click', () => {
	drawingState.tool = 3;
});
document.querySelector('button[name=color-picker]').addEventListener('click', () => {
	drawingState.tool = 16;
});


document.querySelector('#display button[name=clear]').addEventListener('click', () => {
	ctx.fillStyle = secondaryColorPicker.value;
	ctx.fillRect(0, 0, displayCanvas.width, displayCanvas.height);
	
	drawingState.history.push({
		description: `clear`,
		fullImageData: ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height),
	});
	undoButton.disabled = false;
	redoButton.disabled = true;
});

undoButton.addEventListener('click', () => {
	const state = drawingState.history.undo();
	if (state.fullImageData) {
		ctx.putImageData(state.fullImageData, 0, 0);
	}
	else {
		throw new Error('unsupported undo');
	}
	
	undoButton.disabled = !drawingState.history.canUndo();
	redoButton.disabled = false;
});
redoButton.addEventListener('click', () => {
	const state = drawingState.history.redo();
	if (state.fullImageData) {
		ctx.putImageData(state.fullImageData, 0, 0);
	}
	else {
		throw new Error('unsupported redo');
	}
	
	undoButton.disabled = false;
	redoButton.disabled = !drawingState.history.canRedo();
});

document.querySelector('button[name=save]').addEventListener('click', () => {
	const width = 64;
	const height = 32;
	const bytesPerPixel = 2; // 16-bit
	const rowLength = width * bytesPerPixel;
	const paddingPerRow = ((rowLength % 4 > 0) ? (4 - rowLength % 4) : 0);
	const imageSize = (width * bytesPerPixel + paddingPerRow) * height;
	const offsetToPixelArray = 14 + 40 + 12; // file header length + DIB header length + 3x 32-bit masks
	const buffer = new ArrayBuffer(offsetToPixelArray + imageSize);
	const view = new DataView(buffer, 0);
	const LE = true; // to mark little endian usage
	
	// Setup BMP file header
	view.setUint16(0, 0x4D42, LE); // ASCII: 'BM', BMP file signature
	view.setUint32(2, offsetToPixelArray + imageSize, LE); // size in bytes
	view.setUint16(6, 0, LE); // reserved
	view.setUint16(8, 0, LE); // reserved
	view.setUint16(10, offsetToPixelArray, LE); // offset to pixel array
	
	// Setup DIB header
	view.setUint32(14, 40, LE); // the size of this header in bytes
	view.setInt32(18, width, LE);
	view.setInt32(22, height, LE);
	view.setUint16(26, 1, LE); // the number of color planes (must be 1)
	view.setUint16(28, bytesPerPixel * 8, LE); // bits per pixel
	view.setUint32(30, 3, LE); // compression type, 3 (BI_BITFIELDS) means RGB masks should be used
	view.setUint32(34, width * height * bytesPerPixel, LE); // image data size 
	view.setInt32(38, 2835, LE); // the horizontal resolution of the image, pixels per metre
	view.setInt32(42, 2835, LE); // the vertical resolution of the image, pixels per metre
	view.setUint32(46, 0, LE); // the number of colors in the color palette, or 0 to default to 2^n
	view.setUint32(50, 0, LE); // the number of important colors used, or 0 when every color is important; generally ignored
	
	// Setup color bit masks
	view.setUint32(54, 0xF800, LE); // red mask
	view.setUint32(58, 0x07E0, LE); // green mask
	view.setUint32(62, 0x001F, LE); // blue mask
	
	// Copy the pixel data, converting to 16-bit colors
	const uint16Array = new Uint16Array(buffer, offsetToPixelArray);
	const imageData = ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height);
	const dataIterator = imageData.data[Symbol.iterator]();
	for (let y = 0; y < height; y++) {
		for (let x = 0; x < width; x++) {
			const { value: r } = dataIterator.next();
			const { value: g } = dataIterator.next();
			const { value: b } = dataIterator.next();
			const { value: a } = dataIterator.next();
			// if (r === undefined || g === undefined || b === undefined) {
			//    break;
			// }
			uint16Array[width * (height - y - 1) + x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
		}
	}
	
	const blob = new Blob([buffer], { type: 'image/bmp' });
	
	const date = new Date();
	downloadBlob(blob, `image-${date.toISOString().substring(0, 19).replace(/[:-]/g,'')}.bmp`);
	
	// TODO: dialog: to browser (blob), clipboard or microcontroller filesystem
});
document.querySelector('button[name=load]').addEventListener('click', () => {
	// TODO: dialog: from browser, clipboard or microcontroller filesystem
	// TODO: convert from 16-bit BMP if necessary
});

for (const button of document.querySelectorAll('button.control-container')) {
	const input = button.querySelector('input, select');
	button.addEventListener('focus', (e) => e.target != input && input.focus());
	button.addEventListener('click', (e) => e.target != input && input.click());
}

console.log(new Date())
	
/* TODO:
	Display section:
	+ secondary canvas stacked on top, for grid, selection marking etc
	+ select & move tool
	+ paste/copy selection
	+ save as 16-bit BMP (done), with dialog: download (done) or save on the microcontroller file-system
	+ load (well, paste at (0,0)), from upload, URL or read from the microcontroller file-system
	+ keyboard shortcuts
	+ responsive design & make it work on phone
	+ refactor
	+ highlight selected tool
	+ force 16-bit colors?
	
	File-system view:
	+ Basic controls/listing
	+ Uploading
	+ Downloading
	+ Previewing images
	+ Editing animations (as frames + config)
	+ Editing page configs
	+ Select current page config
	
	
*/