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

/// Util to download file 
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

/// Util to open upload file dialog
async function openUploadFiles(accept, multiple = false) {
	const input = document.createElement('input');
	input.type = 'file';
	input.accept = accept;
	input.multiple = multiple;
	input.style.display = 'none';
	document.body.appendChild(input);
	const changed = new Promise((resolve, reject) => { input.addEventListener('change', resolve); });
	input.click();
	await changed;
	input.remove();
	// setTimeout(() => input.remove(), 1);
	return input.files;
}

/// Util to normalize rectangle coords so that first point is in left-top and second in right-bottom,
/// also returning width/height. 
function normalizeRectangleCoords(x0, y0, x1, y1) {
	const ax = Math.min(x0, x1);
	const ay = Math.min(y0, y1);
	const bx = Math.max(x0, x1);
	const by = Math.max(y0, y1);
	return [ax, ay, bx, by, bx - ax + 1, by - ay + 1];
}

// Display canvas represents the state of the display
const displayCanvas = document.querySelector('#display canvas[name=real]');
displayCanvas.width = 64;
displayCanvas.height = 32;
const ctx = displayCanvas.getContext('2d', { alpha: false, willReadFrequently: true });
ctx.imageSmoothingEnabled = false;

// Some stuff for testing
ctx.fillStyle = 'rgb(200, 0, 0)'; ctx.fillRect(0, 0, 1, 1);
drawBresenhamLine(ctx, 3, 4, 8, 9);
ctx.fillStyle = 'white'; ctx.fillRect(0, 24, 32, 8);
ctx.fillStyle = 'black'; ctx.fillRect(32, 24, 32, 8);
const grd = ctx.createLinearGradient(0, 0, 64, 0);
grd.addColorStop(0, 'white');
grd.addColorStop(1, 'black');
ctx.fillStyle = grd; ctx.fillRect(0, 16, 64, 8);

// Helper canvas is used to overlay grid, selection marking, etc.
const helperCanvas = document.querySelector('#display canvas[name=helper]');
helperCanvas.width = helperCanvas.offsetWidth;
helperCanvas.height = helperCanvas.offsetHeight;
const hctx = helperCanvas.getContext('2d');
const helperCanvasRatio = helperCanvas.width / displayCanvas.width; 
// TODO: dynamicly update helper canvas size on container resize

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
	replace(state) {
		if (this.canRedo()) {
			this.stack.length = this.position + 1;
		}
		this.stack[this.position] = state;
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
	getCurrent(offset = 0) {
		return this.stack[this.position + offset];
	}
}

const drawingState = {
	tool: 0, // 0 - none, 1 - pixel, 2 - line, 3 - rectangle, 15 - moving selection, 16 - color picker, 17 - selecting
	pressed: false,
	dragging: false,
	selection: false,
	history: new History({
		description: 'initial',
		fullImageData: ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height),
	}),
}

function drawHelperCanvas() {
	const hcr = helperCanvasRatio;
	hctx.clearRect(0, 0, helperCanvas.width, helperCanvas.height);
	
	hctx.fillStyle = '#7F7F7F1F';
	const lastX = hcr * (displayCanvas.width - 1);
	for (let x = hcr; x <= lastX; x += hcr) {
		hctx.fillRect(x, 0, 1, helperCanvas.height);
	}
	const lastY = hcr * (displayCanvas.height - 1);
	for (let y = hcr; y <= lastY; y += hcr) {
		hctx.fillRect(0, y, helperCanvas.width, 1);
	}
	
	if ((drawingState.tool == 15 || drawingState.tool == 17) && drawingState.selection) {
		let { ax, ay, w, h } = drawingState.selection;
		ax *= hcr;
		ay *= hcr;
		w *= hcr;
		h *= hcr;
		hctx.fillStyle = '#FFFFFFAF'; 
		hctx.fillRect(ax, ay, w, 1);
		hctx.fillRect(ax, ay + h, w, 1);
		hctx.fillRect(ax, ay, 1, h);
		hctx.fillRect(ax + w, ay, 1, h);
	}
}

drawHelperCanvas();

function saveColor(x, y, which) { 
	const rgb = Array.prototype.slice.call(ctx.getImageData(x, y, 1, 1).data, 0, 3);
	const hex = '#' + rgb.map(x => x.toString(16).padStart(2, 0)).join('');
	switch (which) {
		case 0:
			primaryColorPicker.value = hex;
			return;
		case 2:
			secondaryColorPicker.value = hex;
			return;
		default:
			return;
	}
}

function ensureLastHistoryStateHasFullImageData() {
	const state = drawingState.history.getCurrent();
	if (!state.fullImageData) {
		state.fullImageData = ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height);
	}
}

helperCanvas.addEventListener('mousedown', function(e) {
	if (drawingState.tool == 0) return;
	const x = Math.round(e.offsetX / displayCanvas.clientWidth * displayCanvas.width - 0.5);
	const y = Math.round(e.offsetY / displayCanvas.clientHeight * displayCanvas.height - 0.5);
	drawingState.pressed = {x, y};
	if (drawingState.tool == 16) {
		saveColor(x, y, event.button);
		return;
	}
	if (drawingState.tool == 15 || drawingState.tool == 17) {
		if (drawingState.selection) {
			const { ax, ay, bx, by, w, h } = drawingState.selection;
			if (ax <= x && x <= bx && ay <= y && y <= by) {
				drawingState.tool = 15;
				if (drawingState.selection.committed) {
					drawingState.history.undo();
				}
				else {
					drawingState.selection.imageData = ctx.getImageData(ax, ay, w, h);
					ensureLastHistoryStateHasFullImageData();
				}
				return
			}
			else {
				drawingState.selection = false;
				drawHelperCanvas();
			}
		}
		drawingState.tool = 17;
		return;
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
	if (drawingState.tool == 1) {
		ctx.fillRect(x, y, 1, 1);
	}
	else {
		ensureLastHistoryStateHasFullImageData();
	}
});
helperCanvas.addEventListener('mousemove', function(e) {
	if (!drawingState.pressed) return;
	if (drawingState.tool == 0) return;
	const x = Math.round(event.offsetX / displayCanvas.clientWidth * displayCanvas.width - 0.5);
	const y = Math.round(event.offsetY / displayCanvas.clientHeight * displayCanvas.height - 0.5);
	
	if (drawingState.dragging && drawingState.dragging.x == x && drawingState.dragging.y == y) return;
	drawingState.dragging = {x, y};
	
	if (drawingState.tool == 16) {
		saveColor(x, y, event.button);
		return;
	}
	if (drawingState.tool == 17) {
		const [ax, ay, bx, by, w, h] = normalizeRectangleCoords(drawingState.pressed.x, drawingState.pressed.y,
																														drawingState.dragging.x, drawingState.dragging.y);
		drawingState.selection = { ax, ay, bx, by, w, h };
		drawHelperCanvas();
		return;
	}

	if (drawingState.tool != 1) {
		const state = drawingState.history.getCurrent();
		// console.log(drawingState.history.position, state)
		ctx.putImageData(state.fullImageData, 0, 0);
	}
	if (drawingState.selection?.initial) {
		drawingState.selection = drawingState.selection.initial;
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
				const [ax, ay, bx, by, w, h] = normalizeRectangleCoords(x0, y0, x, y);
				ctx.fillRect(ax, ay, w, h);
				// ctx.fillRect(x0 + (x < x0 ? 1 : 0), 
				//              y0 + (y < y0 ? 1 : 0), 
				//              x - x0 + (x < x0 ? -1 : 1), 
				//              y - y0 + (y < y0 ? -1 : 1));
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
		case 15:
			// ctx.fillStyle = 'pink'
			ctx.fillStyle = secondaryColorPicker.value;
			const { ax, ay, w, h } = drawingState.selection;
			if (!drawingState.selection.pasted) {
				ctx.fillRect(ax, ay, w, h);
			}
			ctx.putImageData(drawingState.selection.imageData, x, y);
			drawingState.selection = {
				...drawingState.selection,
				ax: x, ay: y, 
				bx: x + w - 1, by: y + h - 1,
				initial: drawingState.selection,
			};
			drawHelperCanvas();
			break;
	}
});
helperCanvas.addEventListener('mouseup', function() {
	if (0 < drawingState.tool && drawingState.tool < 16) {
		// Save after drawing or moving
		// const shouldReplace = drawingState.tool == 15 && drawingState.selection.committed;
		const description = drawingState.selection?.pasted ? 'paste' : drawingState.tool == 15 ? 'move' : 'draw';
		const fullImageData = ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height);
		// if (shouldReplace) {
		//   drawingState.history.replace({ description, fullImageData });
		// }
		// else {
			drawingState.history.push({ description, fullImageData });
			if (drawingState.tool == 15) {
				drawingState.selection.committed = true;
				// if (drawingState.selection.initial) {
				//   drawingState.selection.initial.committed = true;
				// }
			}
		// }
		undoButton.disabled = false;
		redoButton.disabled = true;
	}
	
	drawingState.pressed = false;
	drawingState.dragging = false;
});
helperCanvas.addEventListener("contextmenu", e => e.preventDefault());

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
document.querySelector('button[name=select]').addEventListener('click', () => {
	drawingState.tool = 17;
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
	
	if (drawingState.selection) {
		if (drawingState.selection.pasted) {
			drawingState.selection = false;
		}
		else {
			drawingState.selection = drawingState.selection.initial;
		}
		drawHelperCanvas();
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
	
	if (drawingState.selection) {
		drawingState.selection = undefined;
		drawHelperCanvas();
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
document.querySelector('button[name=load]').addEventListener('click', async () => {
	// TODO: dialog: from browser, clipboard or microcontroller filesystem
	
	const files = await openUploadFiles();
	if (files.length == 0) return;
	const file = files[0];
	
	const image = new Image();
	const loaded = new Promise((resolve, reject) => {
		image.addEventListener('load', resolve);
		image.addEventListener('error', reject);
	});
	image.src = URL.createObjectURL(file);
	await loaded;

	ctx.drawImage(image, 0, 0);
	drawingState.history.push({
		description: 'paste',
		fullImageData: ctx.getImageData(0, 0, displayCanvas.width, displayCanvas.height)
	});
	undoButton.disabled = false;
	redoButton.disabled = true;
	
	drawingState.tool = 15;
	drawingState.selection = {
		ax: 0, ay: 0,
		bx: image.width - 1, by: image.height - 1,
		w: image.width, h: image.height,
		pasted: true,
		committed: true,
		imageData: ctx.getImageData(0, 0, image.width, image.height),
	};
	drawHelperCanvas();
});

for (const button of document.querySelectorAll('button.control-container')) {
	const input = button.querySelector('input, select');
	button.addEventListener('focus', (e) => e.target != input && input.focus());
	button.addEventListener('click', (e) => e.target != input && input.click());
}

console.log(new Date())

/* TODO:
	Display section:
	+ save only selected, if there is selection
	+ paste/copy selection
	+ keyboard shortcuts
	+ highlight selected tool
	+ save as 16-bit BMP (done), with dialog: download (done) or save on the microcontroller file-system
	+ load from upload, URL or read from the microcontroller file-system
	+ transforms: resizing/scaling, flipping, rotating, skewing?
	+ responsive design & make it work on phone
	+ refactor
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