function esc(str) { return str.replace(/[#;&,\.\+\*~':"!\^\$\[\]\(\)=>|\/\\]/g, '\\$&'); }

function getQuerystring(key, default_)
{
    if (default_==null) default_="";
    key = key.replace(/[\[]/,"\\\[").replace(/[\]]/,"\\\]");
    var regex = new RegExp("[\\?&]"+key+"=([^&#]*)");
    var qs = regex.exec(window.location.href);
    if(qs == null)
	return default_;
  else
      return qs[1];
}

function addItem(s) {
    $('#items').prepend('<tr>' + s + '</tr>');
}

function signif(sensordat, sensorid, name, value, tim, threshold)
{
    if(sensordat[ sensorid + name ].length == 0)
        return 1;
    if(tim - sensordat[ sensorid + name ][sensordat[ sensorid + name ].length-1][0] > ((60*5)-1)*1000)
        return 1;
    if(Math.abs(sensordat[ sensorid + name ][sensordat[ sensorid + name ].length-1][1] - value > threshold))
        return 1;
    return 0;
}

function insertline(line, sensordat) {
    var elem, sensorid;
    var i, temp, tim, rssi, rh, vmcu;

    tim=1;
    elem = line.split(' ');
    if(elem.length < 4) return null;
    //            try {
    for(i=0;i<elem.length;i++) { 
        if(elem[i].substr(0,3) == "UT=")
	    tim=parseInt(elem[i].substr(3))*1000;
        if(elem[i].substr(0,3) == "ID=") {
	    sensorid = elem[i].substr(3);
	    if(localStorage[sensorid] !== undefined) {
		sensorid = localStorage[sensorid];
	    }
	}
        if(elem[i].substr(0,2) == "T=")
	    temp=parseFloat(elem[i].substr(2));
        if(elem[i].substr(0,3) == "RH=")
	    rh=parseFloat(elem[i].substr(3));
        if(elem[i].substr(0,5) == "RSSI=")
	    rssi=parseFloat(elem[i].substr(5));
        if(elem[i].substr(0,6) == "V_MCU=")
	    vmcu=parseFloat(elem[i].substr(6))*10;
    }
    //            } catch(err) { return null; }
    if(tim == 1) return null;
    if(!sensorid) return null;

    if(sensordat[ sensorid + "-init" ] === undefined) {
	sensordat[ sensorid + "-init" ] = true;
	sensordat[ sensorid + "-temp" ] = [ ];
	sensordat[ sensorid + "-rssi" ] = [ ];
	sensordat[ sensorid + "-rh" ] = [ ];
	sensordat[ sensorid + "-vmcu" ] = [ ];
	sensordat[ "sensors" ].push(sensorid);
	sensordat[ "series" ].push(sensorid+"-temp");
	sensordat[ "series" ].push(sensorid+"-rh");
	sensordat[ "series" ].push(sensorid+"-rssi");
	sensordat[ "series" ].push(sensorid+"-vmcu");
	sensordat[ sensorid + "-rh" ].yaxis = 2;
	sensordat[ sensorid + "-temp" ].yaxis = 1;
    }

    if(temp !== undefined) { 
	if(signif(sensordat, sensorid, "-temp", temp, tim, 0.2) == 1)
	    sensordat[ sensorid + "-temp" ].push( [ tim, temp ] );
    }
    if(rh !== undefined) { 
	if(signif(sensordat, sensorid, "-rh", rh, tim, 0.2) == 1)
	    sensordat[ sensorid + "-rh" ].push( [ tim, rh ] );
    }
    if(rssi !== undefined) { 
	if(signif(sensordat, sensorid, "-rssi", rssi, tim, 0.2) == 1)
	    sensordat[ sensorid + "-rssi" ].push( [ tim, rssi ] );
    }
    if(vmcu !== undefined) { 
	if(signif(sensordat, sensorid, "-vmcu", vmcu, tim, 0.5) == 1)
            sensordat[ sensorid + "-vmcu" ].push( [ tim, vmcu ] );
    }
}

function insertdata(text, sensordat) {
    var i;
    var elem;
    var narr = [ ];
    var arr = text.split('\n');
    for(i=0;i<arr.length;i++) {
        insertline(arr[i], sensordat);
    }
}

function convertline(line) {
    var elem;
    var i, temp, tim;
    var value = { };

    tim=1;
    elem = line.split(' ');
    if(elem.length < 4) return null;
    //            try {
    for(i=0;i<elem.length;i++) { 
        if(elem[i].substr(0,3) == "UT=")
	    tim=parseInt(elem[i].substr(3))*1000;
        if(elem[i].substr(0,3) == "ID=")
	    value["id"] = elem[i];
        if(elem[i].substr(0,2) == "T=")
            temp=parseFloat(elem[i].substr(2));
    }
    //            } catch(err) { return null; }
    if(tim == 1) return null;
    if(temp == undefined) return null;
    value[ "tuple" ] = [ tim, temp ];
    return value;
}

function convertdata(text) {
    var i;
    var elem;
    var narr = [ ];
    var arr = text.split('\n');
    for(i=0;i<arr.length;i++) {
        elem = convertline(arr[i]);
        if(elem != null) {
	    narr.push(elem["tuple"]);
	}
    }
    return narr;
}

function binarysearch(url, target, start, end) {
    var point;
    var r;
    var iter=0;

    // Type conversion
    start=start-0;
    end=end-0;
    
    console.log("end = " + end);

    while(1) {
	if(iter++ > 10) break;
	
	// get (start+end)/2
	point = Math.floor((start+end)/2);
	
	a = $.ajax({
	    beforeSend: function(xhrObj){
		var rh;
		rh = "bytes=" + point + "-" + (point+256);
		xhrObj.setRequestHeader("Range", rh);
	    },
            async: false,
            url: url,
            method: 'GET',
            dataType: 'text',
	});
	
	// convert
	r = convertdata(a.responseText);
	if(r.length == 0) break;
	if(r[0][0] == target) return point;
	if(r[0][0] > target) end=point;
	if(r[0][0] < target) start=point;
    }
    return start;
}

function drawplot(options) {
    var a;
    var placeholder = $("#placeholder");
    var span;
    span = parseInt(getQuerystring('span'))*60*1000;
    if(span < 1000) span = 5*60*1000;

    
    // find the URL in the link right next to us 
    var dataurl = "sensor.dat";
    
    // then fetch the data with jQuery
    function onDataReceived(rawseries, status, xhr) {
        // extract the first coordinate pair so you can see that
        // data is now an ordinary Javascript object
        var arr, i, step;
        var firstcoordinate;
	var sensordat = { };
	sensordat["sensors"] = [ ];
	sensordat["series"] = [ ];
	
	insertdata(rawseries, sensordat);
	data = [];

	while($('#items tr').length > 0) {
	    $('#items').children().eq(0).remove();
	}
	
	for(i=0;i<sensordat["series"].length;i++) {
	    var series = { };

	    series.label = sensordat["series"][i];

	    series.data = sensordat[sensordat["series"][i]];
	    if(sensordat[sensordat["series"][i]]) {
		if(sensordat[sensordat["series"][i]].yaxis) {
		    series.yaxis = sensordat[sensordat["series"][i]].yaxis;
		}
		var ischecked = "checked";
		var value;
		if(series.data[series.data.length-1]) value = series.data[series.data.length-1][1];
		else value="na";
		if(localStorage[series.label] == "no") ischecked = "";
		addItem('<td><input type="checkbox" name="' + series.label + '" id="' + esc(series.label) + '" ' + ischecked + ' />' + series.label + "</td><td>" + value +"</td>");
		$('[name="' + series.label + '"]' ).click(function(label) {
		    return function () {
			if(this.checked) {
			    localStorage[label] = "yes";
			} else {
			    localStorage[label] = "no";
			}
		    }
		}(series.label));
		if(value != "na") {
		    if(localStorage[series.label] != "no")
			data.push(series);
		}
	    }
	}
	
        // and plot all we got
	options.xaxis = {  axisLabelUseCanvas:true, axisLabel: 'Date', min: Date.now() - span, max: Date.now()-0, mode: "time", timeformat: "%d/%m %h:%M" };
        $.plot(placeholder, data, options);
    }
    
    a = $.ajax({
	beforeSend: function(xhrObj){
	    var rh;
	    rh = "bytes=0-2";
	    xhrObj.setRequestHeader("Range", rh);
	},
        url: dataurl,
        method: "GET",
        async: false,
        dataType: 'text'
    });
    var datasize = parseInt(a.getResponseHeader('Content-Range').split('/')[1]);
    
    start = binarysearch(dataurl, Date.now() - span, 0, datasize-0);
    console.log("starting at " + start);
    
    a = $.ajax({
	beforeSend: function(xhrObj){
	    var rh;
	    rh = "bytes=" + start + "-" + datasize;
	    xhrObj.setRequestHeader("Range", rh);
	},
        url: dataurl,
        method: 'GET',
        dataType: 'text',
        success: onDataReceived
    });
};
    


$(function () {
    // shorthand for: $(document).ready(callback)
    
    var options = {
        lines: { show: true },
        yaxes: [ { panRange: false, axisLabelUseCanvas:true, axisLabel: 'Temperature C' , position: 'right'}, { panRange: false, axisLabelUseCanvas:true, axisLabel: 'Relative humidity', min: 0, max: 100, position: 'left' } ],
	legend: { position: 'nw' },
        pan: {
            interactive: true
        }
    };
    var data = [];
    var span;
    var placeholder = $("#placeholder");

    var loc = $(location).attr('href');
    $('a#back').attr('href', loc.substring(0, loc.lastIndexOf('/') + 1));

    span = parseInt(getQuerystring('span'))*60*1000;
    if(span < 1000) span = 5*60*1000;

    document.cookie = "temperature=20; max-age=" + 60*60*24*365;

    $.plot(placeholder, data, options);
    
    // fetch one series, adding to what we got
    var alreadyFetched = {};
    
    drawplot(options);
    $("input.fetchSeries").click(function () {
	drawplot(options);
    });
    
    // initiate a recurring data update
    $("input.dataUpdate").click(function () {
        // reset data
        data = [];
        alreadyFetched = {};
        
        $.plot(placeholder, data, options);
	
        var iteration = 0;
        
        function fetchData() {
            ++iteration;
	    
            function onDataReceived(series) {
                // we get all the data in one go, if we only got partial
                // data, we could merge it with what we already got
                data = [ series ];
                
                $.plot($("#placeholder"), data, options);
            }
            
            $.ajax({
                // usually, we'll just call the same URL, a script
                // connected to a database, but in this case we only
                // have static example files so we need to modify the
                // URL
                url: "data-eu-gdp-growth-" + iteration + ".json",
                method: 'GET',
                dataType: 'json',
                success: onDataReceived
            });
            
            if (iteration < 5)
                setTimeout(fetchData, 1000);
            else {
                data = [];
                alreadyFetched = {};
            }
        }
	
        setTimeout(fetchData, 1000);
    });
});
