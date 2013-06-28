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
}

var xhr = new XMLHttpRequest();
function xhr_handler() {
	if (xhr.readyState === 4) {
		if (xhr.status === 200) {
			data = JSON.parse(xhr.responseText);
/*			for (attr in data) {
				console.log('xhr attr', attr);
			} */
			update_elements(data);
		} else {
			console.log('xhr: something went wrong', xhr.status, xhr.statusText);
		}
	}
}
xhr.onreadystatechange = xhr_handler;

function timer() {
	xhr.open('GET', '/');
	// xmlhttp.setRequestHeader('X-Requested-With', 'XMLHttpRequest');
	xhr.send(null);
	window.setTimeout(timer, 2000);
}

function doload() {
	prepare_cliprects();
	timer();
}
