angular.module('cardDemo1')
    .factory('WebsocketFact', function($websocket) {
        var collection = [];
        var ws = $websocket();
        return {
            start: function(data) {
                ws = $websocket(data);
                ws.onError(function(event) {
                    console.log('connection Error', event);
                });
                ws.onClose(function(event) {
                    console.log('connection closed', event);
                });
                ws.onOpen(function() {
                    console.log('connection open');
                });
            },
            stopConnection: function() {
                ws.close();
            },
            collection: collection,
            syncUpdates: function(datas, cb) {
                cb = cb || angular.noop;
                ws.onMessage(function(event) {
                    var now = {};
                    now.timestamp = new Date();
                    now.data = event.data;
                    collection.push(now);
                    cb(datas, event);
                });
            },
            send: function(message) {
                if (angular.isString(message)) {
                    ws.send(message);
                } else if (angular.isObject(message)) {
                    ws.send(JSON.stringify(message));
                }
            }
        };
    });