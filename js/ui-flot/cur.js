function addItem(s) {
    $('#items').prepend('<tr>' + s + '</tr>');
}
function addSensor(id, s) {
    $('#sensors').prepend('<li><input type="text" size="30" name="' + id + '" value="' + s + '" disabled /><tt id="' + id + '">edit</tt> <tt id="' + id + 'save">save</tt></li>');
    $("#" + id).click(function() {
	$('input[name=' + id +']').removeAttr('disabled');
//	alert("Handler for .click(" + id + ") called:" + s);
    });
    $("#" + id + "save").click(function() {
	$('input[name=' + id +']').attr('disabled', true);
	localStorage[id] = $('input[name=' + id +']').val();
    });
}

function insertline(line, sensordat) {
    var elem, sensorid, sensorname;
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
	    sensorname = localStorage[sensorid];
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

    if(temp !== undefined) { 
	if(!sensordat[ sensorname + "-temp" ]) {
	    sensordat[ sensorname + "-temp" ] = [ ];
	    sensordat[ "sensors" ].push(sensorid);
	    sensordat[ "series" ].push(sensorname+"-temp");
	    sensordat[ "series" ].push(sensorname+"-rh");
	    sensordat[ "series" ].push(sensorname+"-rssi");
	    sensordat[ "series" ].push(sensorname+"-vmcu");
	    sensordat[ sensorname + "-temp" ].yaxis = 1;
	}
	if(sensordat[ sensorname + "-temp" ].length == 0)
	    sensordat[ sensorname + "-temp" ].push( [ tim, temp ] );
	else {
	    if( Math.abs(sensordat[ sensorname + "-temp" ][sensordat[ sensorname + "-temp" ].length-1][1] - temp) > 0.2)
		sensordat[ sensorname + "-temp" ].push( [ tim, temp ] );
	}
    }
    if(rh !== undefined) { 
	if(!sensordat[ sensorname + "-rh" ]) {
	    sensordat[ sensorname + "-rh" ] = [ ];
	    sensordat[ sensorname + "-rh" ].yaxis = 2;
	}
	if(sensordat[ sensorname + "-rh" ].length == 0)
	    sensordat[ sensorname + "-rh" ].push( [ tim, rh ] );
	else {
	    if( Math.abs(sensordat[ sensorname + "-rh" ][sensordat[ sensorname + "-rh" ].length-1][1] - rh) > 0.2)
		sensordat[ sensorname + "-rh" ].push( [ tim, rh ] );
	}
    }
    if(rssi !== undefined) { 
	if(!sensordat[ sensorname + "-rssi" ])
	    sensordat[ sensorname + "-rssi" ] = [ ];
	if(sensordat[ sensorname + "-rssi" ].length == 0)
	    sensordat[ sensorname + "-rssi" ].push( [ tim, rssi ] );
	else {
            if( Math.abs(sensordat[ sensorname + "-rssi" ][sensordat[ sensorname + "-rssi" ].length-1][1] - rssi) > 0.2)
		sensordat[ sensorname + "-rssi" ].push( [ tim, rssi ] );
	}
    }
    if(vmcu !== undefined) { 
	if(!sensordat[ sensorname + "-vmcu" ])
	    sensordat[ sensorname + "-vmcu" ] = [ ];
	sensordat[ sensorname + "-vmcu" ].push( [ tim, vmcu ] );
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

function onDataReceived(data, text) {
    data["sensors"] = [ ];
    data["series"] = [ ];

    insertdata(text, data);
    while($('#items tr').length > 0) {
        $('#items').children().eq(0).remove();
    }
    for(i=0;i<data["sensors"].length;i++) {
	if( (localStorage[data["sensors"][i]] === undefined) ||
	    (localStorage[data["sensors"][i]] === null) ||
	    (localStorage[data["sensors"][i]] == 'null'))
	    localStorage[data["sensors"][i]] = data["sensors"][i];
	addSensor(data["sensors"][i], localStorage[data["sensors"][i]]);
    }
    for(i=0;i<data["series"].length;i++) {
        var series = { };
	try {
	    series.label = data["series"][i];
            series.data = data[data["series"][i]];
            addItem('<td>' + new Date(series.data[series.data.length-1][0]) + "</td><td>" + series.label + "</td><td>" + series.data[series.data.length-1][1]+"</td>"); }
	catch(err) { }; 
    }
}

$(function () {
    // shorthand for: $(document).ready(callback)

    var dataurl = "sensor.dat";    
    var data = [];

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

    start = datasize - 10000;
    
    $.ajax({
        beforeSend: function(xhrObj){
            var rh;
            rh = "bytes=" + start + "-" + datasize;
            xhrObj.setRequestHeader("Range", rh);
        },
        url: dataurl,
        method: 'GET',
        dataType: 'text',
        success: function(text, status, xhr) { onDataReceived(data, text); }
    });

});
