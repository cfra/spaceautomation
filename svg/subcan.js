/* JSON object global ref */
var data = null;

function simple_xpath(expr) {
	var root = document.documentElement;
	var iter = document.evaluate(expr, root,
		document.createNSResolver(root),
		XPathResult.UNORDERED_NODE_ITERATOR_TYPE, null);

	var nodes = [];
	var nwalk = iter.iterateNext();
	while (nwalk) {
		nodes.push(nwalk);
		nwalk = iter.iterateNext();
	}
	return nodes;
}

var picktgt = null;
var picktimer = null;
var picker;

function picker_end() {
	node = simple_xpath('//*[contains(svg:title, "'+picktgt+'=")]')[0];
	node.style.strokeWidth = 1.0;
	node.style.stroke = '#b0b0b0';

	picktgt = null;
	var pickelem = document.getElementById('picker');
	pickelem.style.visibility = 'hidden';
}

function picker_begin(target) {
	if (picktgt) {
		window.clearTimeout(picktimer);
		picker_end();
	}

	node = simple_xpath('//*[contains(svg:title, "'+target+'=")]')[0];
	node.style.strokeWidth = 3.0;
	node.style.stroke = '#ff7700';

	picktgt = target;
	picker.setRgb(data[picktgt]);

	var pickelem = document.getElementById('picker');
	pickelem.style.visibility = 'visible';
	picktimer = window.setTimeout(picker_end, 30000);
}

function picker_refresh() {
	window.clearTimeout(picktimer);
	picktimer = window.setTimeout(picker_end, 30000);
}

function rgb2dmx(r, g, b) {
	r = Math.floor((Math.pow(4.0, r / 255.0) - 1) / 3 * 255);
	g = Math.floor((Math.pow(4.0, g / 255.0) - 1) / 3 * 180);
	b = Math.floor((Math.pow(4.0, b / 255.0) - 1) / 3 * 144);
	return [r,g,b];
}
function dmx2rgb(r, g, b) {
	l4 = 255.0 / Math.log(4.0);
	r = Math.floor(Math.log(r / 255.0 * 3 + 1) * l4);
	g = Math.floor(Math.log(g / 180.0 * 3 + 1) * l4);
	b = Math.floor(Math.log(g / 144.0 * 3 + 1) * l4);
	return [r,g,b];
}

function on_picker(hex, hsv, rgb) {
	console.log('pick', picktgt, rgb);
	picker_refresh();

	$.jsonRPC.request('light_set', {
		params: [picktgt, rgb2dmx(rgb.r, rgb.g, rgb.b)],
		error: function(result) {
			console.log('light_set RGB error', result);
		},
		success: function(result) {
			// console.log('light_set ', id, ' => ', mouseset, ' OK: ', result['result']);
			// immediate_update(id, mouseset);
		}
	});
}


var mousesel = null;
var mouseset = null;
var mouseorig = 0;
var mousey = 0;
var mousetimer = null;
var mousetdir = 0;
var mousestart = 0;

function immediate_update(id, setval) {
	var nodes = simple_xpath('//svg:text[@inkscape:label = "'+id+'=%set"]');

	for (i in nodes) {
		var node = nodes[i];
		value = Math.round(setval / 255. * 100) + "%";
		node.firstChild.textContent = value;
	}
}

function on_evt_click(node) {
	var id = node.id.substring(4);
	var now = new Date().getTime();
	if (now - mousestart > 200)
		return;

	console.log("clicked", id);

	$.jsonRPC.request('light_get', {
		params: [id],
		error: function(result) {
			console.log('light_get', id, 'error', result);
		},
		success: function(result) {
	/* <<< */
	r = result['result']
	console.log('light_get', r);
	var set;
	if (r['r']) {
		set = (r['r'] + r['g'] + r['b']) / 3;
	} else {
		set = r['set'];
	}
	var newset = set ? 0 : 255;
	$.jsonRPC.request('light_set', {
		params: [id, newset],
		error: function(result) {
			console.log('light_set error', result);
		},
		success: function(result) {
			// console.log('light_set ', id, ' => ', newset, ' OK: ', result['result']);
			immediate_update(id, newset);
		}
	});
	/* >>> */
		}
	});
}

function on_mouse_timer() {
	if (mousesel === null)
		return;

	var id = mousesel.id.substring(4);

/*	mouseset += mousetdir * 8;
	if (mouseset < 1)
		mouseset = 1;
	if (id.substring(0, 4) == 'dali' && mouseset < 0x55)
		mouseset = 0x55;
	if (mouseset > 255)
		mouseset = 255; */

	$.jsonRPC.request('light_set', {
		params: [id, mouseset],
		error: function(result) {
			console.log('light_set error', result);
		},
		success: function(result) {
			// console.log('light_set ', id, ' => ', mouseset, ' OK: ', result['result']);
			immediate_update(id, mouseset);
		}
	});

	mousetimer = window.setTimeout(on_mouse_timer, 100);
}

function on_evt_mousedown(node, evt) {
	var id = node.id.substring(4);

	if (id.substr(0,3) == 'dmx') {
		console.log('dmx click', id);
		picker_begin(id);
		return false;
	}

	// console.log('node', node, 'y', evt.clientY);
	mousestart = new Date().getTime();
	node.style.fill = "#0088ff";
	node.style.fillOpacity = 0.25;
	mousesel = node;
	mousey = evt.clientY;
	$.jsonRPC.request('light_get', {
		params: [id],
		error: function(result) {
			console.log('light_get error', result);
		},
		success: function(result) {
			r = result['result']
			if (r['r']) {
				mouseorig = (r['r'] + r['g'] + r['b']) / 3;
			} else {
				mouseorig = r['set'];
			}
			mouseset = mouseorig;
		}
	});

	if (evt.stopPropagation) evt.stopPropagation();
	evt.cancelBubble = true;
	return false;
}

function on_evt_mousemove(node, evt) {
	if (mousesel === null)
		return;

	var offs = evt.clientY - mousey;
	// console.log('node', node, 'set', mouseset, 'y', evt.clientY, 'offs', offs);
	var deadzone = 20;
	var dimzone = 120;

	var min = 1;
	if (mousesel.id.substring(4, 8) == 'dali')
		min = 0x55;

	if (offs > -deadzone && offs < deadzone) {
		if (mousetimer !== null)
			window.clearTimeout(mousetimer);
		mousetimer = null;
		return;
	}

	if (offs < 0) {
		var pos = (-offs - deadzone) / dimzone;
		// console.log("cvo", offs, "+dim", pos);
		mouseset = mouseorig + pos * (255 - mouseorig);
	} else {
		var pos = (offs - deadzone) / dimzone;
		// console.log("cvo", offs, "-dim", pos);
		mouseset = mouseorig * (1.0 - pos);
	}
	mouseset = Math.floor(mouseset);
	if (mouseset > 255)
		mouseset = 255;
	if (mouseset < min)
		mouseset = min;

	if (mousetimer === null)
		mousetimer = window.setTimeout(on_mouse_timer, 100);

	evt.cancelBubble = true;
	if (evt.stopPropagation) evt.stopPropagation();
	return false;
}

function on_evt_mouseup(node, evt) {
	// console.log('--- up ---');
	if (mousesel) {
		window.setTimeout((function() {
			var sel = mousesel;
			return function() {
				sel.style.fillOpacity = 0.0;
			}
		})(), 250);
	}
	mousesel = null;

	evt.cancelBubble = true;
	if (evt.stopPropagation) evt.stopPropagation();
	return false;
}

function prepare_cliprects() {
	var nodes = simple_xpath('//svg:rect[contains(@inkscape:label, "=")]');
	console.log('setup');
	for (i in nodes) {
		var node = nodes[i];
		node.orig_x = node.x.baseVal.value;
		node.orig_y = node.y.baseVal.value;
		node.orig_w = node.width.baseVal.value;
		node.orig_h = node.height.baseVal.value;
	}
}

function prepare_evts() {
	$.jsonRPC.setup({
		endPoint: '/jsonrpc',
		namespace: ''
	});
	$.jsonRPC.request('ping', {
		success: function(result) {
			console.log('ping', result['result']);
		},
		error: function(result) {
			console.log('ping error', result);
		}
	});

	var nodes = simple_xpath('//svg:g[@inkscape:label="events"]/svg:rect');
	console.log('setup events', nodes);
	for (i in nodes) {
		var node = nodes[i];
		var id = node.id;
		console.log('event node', i, " => ", node);
		node.onclick = (function() {
			var current_node = node;
			return function() { on_evt_click(current_node); }
		})();
		node.onmousedown = (function() {
			var current_node = node;
			return function(evt) { on_evt_mousedown(current_node, evt); }
		})();
		node.onmousemove = (function() {
			var current_node = node;
			return function(evt) { on_evt_mousemove(current_node, evt); }
		})();
		node.onmouseup = (function() {
			var current_node = node;
			return function(evt) { on_evt_mouseup(current_node, evt); }
		})();
		node.onselectstart = function() { return false; }
		node.unselectable = 'on';
		node.style.userSelect = 'none';
		node.style.MozUserSelect = 'none';
		node.style.WebkitUserSelect = 'none';
	}
	document.onmouseup = (function() {
		var current_node = node;
		return function(evt) { on_evt_mouseup(current_node, evt); }
	})();
	document.onmousemove = (function() {
		var current_node = node;
		return function(evt) { on_evt_mousemove(current_node, evt); }
	})();

	var nodes = simple_xpath('//svg:text');
	for (i in nodes) {
		var node = nodes[i];
		node.onselectstart = function() { return false; }
		node.unselectable = 'on';
		node.style.userSelect = 'none';
		node.style.MozUserSelect = 'none';
		node.style.WebkitUserSelect = 'none';
	}
}

function update_cliprect(node, dir, val) {
	//var cliprect = document.getElementById("cliprect-dali.lounge_buero-0");
	//var t = new Date().getTime() / 2000 * Math.PI;
	// console.log("clipper: ", cliprect, " height: ", cliprect.height.baseVal.value, " sin: ", Math.sin(t));
	// var state = 0.5 * (1 + Math.sin(t));

	//console.log('node', node, 'dir', dir, 'val', val);
	if (dir === "y") {
		//console.log('orig_y', node.orig_y, 'orig_h', node.orig_h);
		node.y.baseVal.value      = node.orig_y + node.orig_h * (1 - val);
		node.height.baseVal.value = node.orig_h * val;
	} else if (dir === 'x') {
		//console.log('orig_x', node.orig_x, 'orig_w', node.orig_w, 'w', node.orig_w * val);
		node.width.baseVal.value  = node.orig_w * val;
	}
}

function update_elements(json) {
	var nodes = simple_xpath('//*[contains(@inkscape:label, "=")]');

	for (i in nodes) {
		var node = nodes[i];
		var spec = node.attributes['inkscape:label'].value.split('=', 2);
		var dataelem = json[spec[0]];

		if (dataelem === null)
			continue;

		if (node.localName == "g") {
			node.style.display = (dataelem.text == spec[1])
				? "inline" : "none";
		} else if (node.localName == "text") {
			var field = spec[1];
			var value;
			if (field[0] == '%') {
				field = field.slice(1);
				value = Math.round(dataelem[field] / 255. * 100) + "%";
			} else {
				value = dataelem[field];
			}
			node.firstChild.textContent = value;
		} else if (node.localName == "rect") {
			update_cliprect(node, spec[1], dataelem['actual'] / 255.);
		} else {
			console.log("unknown dynamic content type", node.localName);
		}
	}

	nodes = simple_xpath('//*[contains(svg:title, "=")]');
	for (i in nodes) {
		var node = nodes[i];
		var spec = node.getElementsByTagName('title')[0].firstChild.nodeValue.split('=', 2);
		var dataelem = json[spec[0]];

		if (dataelem === null)
			continue;

		if (node.localName == "path") {
			rgb = dmx2rgb(dataelem['r'], dataelem['g'], dataelem['b']);
			node.style.fill = "rgb("
				+ rgb[0] + ", "
				+ rgb[1] + ", "
				+ rgb[2] + ")";
		} else {
			console.log("unknown dynamic content type", node.localName);
		}
	}
}

var xhr = new XMLHttpRequest();
var xhrtimeout = null;
var xhrlastref = null;

function xhr_handler() {
	if (xhr.readyState === 4) {
		if (xhr.status === 200) {
			if (xhr.responseText != '') {
				rdata = JSON.parse(xhr.responseText);
				data = rdata['data'];
				xhrlastref = rdata['ref'];
			}
			console.log('xhr OK', xhrlastref);

			timer(0);

/*			for (attr in data) {
				console.log('xhr attr', attr);
			} */
			update_elements(data);
		} else if (xhr.status === 0) {
			/* request abort, ignore */
		} else {
			console.log('xhr: something went wrong', xhr.status, xhr.statusText);
		}
	}
}
xhr.onreadystatechange = xhr_handler;

function ISOTS() {
	function pad(n){return n<10 ? '0'+n : n}
	var d = new Date();
	return d.getUTCFullYear()+'-'
		+ pad(d.getUTCMonth()+1)+'-'
		+ pad(d.getUTCDate())+'T'
		+ pad(d.getUTCHours())+':'
		+ pad(d.getUTCMinutes())+':'
		+ pad(d.getUTCSeconds())+'Z';
}

function timer(expired) {
	var dbgtext = document.getElementById('dbgtextreal');

	if (expired) {
		xhr.abort();
	} else {
		window.clearTimeout(xhrtimeout);
	}

	if (xhrlastref === null) {
		xhr.open('GET', '/longpoll');
		dbgtext.textContent = ISOTS() + ' (none)';
	} else {
		xhr.open('GET', '/longpoll?' + xhrlastref);
		dbgtext.textContent = ISOTS() + ' ' + xhrlastref;
	}
	// xmlhttp.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
	xhr.send(null);
	xhrtimeout = window.setTimeout(timer, 45000, 1);
}

function doload() {
	prepare_cliprects();
	prepare_evts();
	timer();
}
