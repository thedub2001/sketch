angular.module('cardDemo1')
    .factory('socket', function(socketFactory) {
        var ioSocket = io('http://78.218.62.97:8057', {
            path: '/socket.io-client'
        });
        var socket = socketFactory({
            ioSocket: ioSocket
        });
        return {
            socket: socket,
            sendMessage: function(message) {
                socket.emit('esp', message);
                console.log('sent');
                console.log(message);
            },
            syncUpdates: function(modelName, array, cb) {
                cb = cb || angular.noop;

                socket.on(modelName + ':save', function(item) {
                    console.log('socket:save');
                    console.log(item);
                    cb(item, array);
                });

                socket.on(modelName + ':remove', function(item) {
                    console.log('socket:remove');
                    cb(item, array);
                });
            },
            unsyncUpdates: function(modelName) {
                socket.removeAllListeners(modelName + ':save');
                socket.removeAllListeners(modelName + ':remove');
            }
        };
    });