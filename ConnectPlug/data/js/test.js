'use strict';

var _ = require('lodash');
var Esp = require('./esp.model');


var crypto = require('crypto'),
    algorithm = 'aes-256-ctr',
    password = 'd6F3Efeq';

function encrypt(text, pw) {
    var cipher = crypto.createCipher(algorithm, pw);
    var crypted = cipher.update(text, 'utf8', 'hex');
    crypted += cipher.final('hex');
    return crypted;
}

function decrypt(text, pw) {
    var decipher = crypto.createDecipher(algorithm, pw);
    var dec = decipher.update(text, 'hex', 'utf8');
    dec += decipher.final('utf8');
    return dec;
}

var ipList = [];
for (var i = 1; i < 50; i++) {
    var urrl = 'http://192.168.1.' + i + '/relay';
    ipList.push(urrl);
};





var makeRequests = function() {
    var deferred = Q.defer();
    var promises = [];
    _.forEach(ipList, function(i) {
        var def = Q.defer();
        var ip = i;
        console.log('start ' + ip);
        rp(ip).then(function(response) {
            console.log(response);
            var newEsp = {
                'iotname': response.iotname,
                'iotid': response.iotid,
                'r1': response.R1,
                'current': response.current,
                'temperature': response.temperature,
                'humidity': response.humidity
            };

            Esp.create(newEsp, function(err, esp) {
                if (err) { console.log(err); }
                console.log(dd);
            });
        }).catch(function(err) {

        });
    });
    console.log('fini');
};

// Get list of esps
exports.index = function(req, res) {
    Esp.find(function(err, esps) {
        if (err) { return handleError(res, err); }
        return res.status(200).json(esps);
    });
};

exports.scan = function(req, res) {
    makeRequests();
    return res.status(200).json("ok");
};

// Get a single esp
exports.show = function(req, res) {
    Esp.findById(req.params.id, function(err, esp) {
        if (err) { return handleError(res, err); }
        if (!esp) { return res.status(404).send('Not Found'); }
        return res.json(esp);
    });
};

exports.historic = function(req, res) {
    console.log(req.params.iotid);
    Esp.find().sort('created').find({ iotid: req.params.iotid }, function(err, historics) {
        if (err) { return handleError(res, err); }
        if (!historics) { return res.status(404).send('Not Found'); }
        return res.status(200).json(historics);
    });
};


exports.addNew = function(req, res) {
    var newEsp = {
        'iotname': req.params.iotname,
        'iotid': req.params.iotid,
        'r1': req.params.r1,
        'r2': req.params.r2,
        'r3': req.params.r3,
        'r4': req.params.r4,
        'current': req.params.current,
        'temperature': req.params.temperature,
        'humidity': req.params.humidity
    };
    var passwords = req.params.iotid;
    //var text=encrypt(JSON.stringify(newEsp),password);
    //console.log(text);
    //console.log(decrypt(text,password));
    Esp.create(newEsp, function(err, esp) {
        var dd = { 'data': 'ok' };
        if (err) { return handleError(res, err); }
        return res.status(201).json(dd);
    });
};


// Creates a new esp in the DB.
exports.create = function(req, res) {
    //console.log(req);
    console.log(req.headers);
    console.log("POST from " + req.body.iotname)
    console.log(req.body);
    if (req.body.iotid) {
        Esp.create(req.body, function(err, esp) {
            if (err) { return handleError(res, err); }
            return res.status(201).json(esp);
        });
    } else {
        return res.status(201).json("empty");
    }
};

// Updates an existing esp in the DB.
exports.update = function(req, res) {
    if (req.body._id) { delete req.body._id; }
    Esp.findById(req.params.id, function(err, esp) {
        if (err) { return handleError(res, err); }
        if (!esp) { return res.status(404).send('Not Found'); }
        var updated = _.merge(esp, req.body);
        updated.save(function(err) {
            if (err) { return handleError(res, err); }
            return res.status(200).json(esp);
        });
    });
};

// Deletes a esp from the DB.
exports.destroy = function(req, res) {
    Esp.findById(req.params.id, function(err, esp) {
        if (err) { return handleError(res, err); }
        if (!esp) { return res.status(404).send('Not Found'); }
        esp.remove(function(err) {
            if (err) { return handleError(res, err); }
            return res.status(204).send('No Content');
        });
    });
};

function handleError(res, err) {
    return res.status(500).send(err);
}