angular.module('cardDemo1', ['ngMaterial', 'chart.js', 'ngWebSocket', 'ngAnimate', 'ui.router'])
    .controller('AppCtrl', function($scope, $http, $interval, WebsocketFact, $mdSidenav) {
        $scope.toggleLeft = buildToggler('left');
        $scope.toggleRight = buildToggler('right');
        $scope.allSockets = {};
        $scope.allEsp = [];

        function buildToggler(componentId) {
            return function() {
                $mdSidenav(componentId).toggle();
            }
        };

        $scope.labelsRadar = ["Luminosite", "Consommation", "temperature ", "humidity"];
        $scope.dataRadar = {};

        $scope.labelsGraph = {};

        $scope.seriesGraph = ["Luminosite", "Consommation", "temperature ", "humidity"];
        $scope.dataGraph = {};

        $scope.onClick2 = {};



        $scope.datasetOverride = [{ yAxisID: 'y-axis-1' }, { yAxisID: 'y-axis-2' }];
        $scope.options2 = {
            animation: {
                duration: 0
            },
            elements: {
                line: {
                    borderWidth: 0.5
                },
                point: {
                    radius: 0
                }
            },
            scales: {
                yAxes: [{
                        id: 'y-axis-1',
                        type: 'linear',
                        display: true,
                        position: 'left'
                    },
                    {
                        id: 'y-axis-2',
                        type: 'linear',
                        display: false,
                        position: 'right'
                    }
                ]
            }
        };








        $scope.imagePath = 'hello.png';
        $scope.capteurs = [];
        $scope.dataWs = {};
        $scope.imageName = ['Image0', 'Image1', 'Image2', 'Image3', 'Image4', 'Image5'];
        $scope.imageURL = ['img/energy.svg', 'img/switchOn.svg', 'img/switchOff.svg', 'img/temp.svg', 'img/hum.svg', 'img/lux.svg'];
        $scope.lesDatas = {};
        $scope.mesSent = {};
        $scope.Messages = WebsocketFact;
        $scope.theUrl = '78.218.62.97:81';
        $scope.connected = 0;
        $scope.goodNetwork = '';
        $scope.networkk = [];




        $scope.startWebsocketS = function(url, name) {
            $scope.allSockets[name] = WebsocketFact;
            $scope.allSockets[name].start('ws://' + url + '/');
            $scope.allSockets[name].syncUpdates($scope.lesDatas, function(datas, messages) {
                $scope.connected = 1;
                console.log('Sync UpdateS - ' + name);
                var dataTemp = JSON.parse(messages.data);
                if (dataTemp.button !== undefined) {
                    if (dataTemp.temperature !== '998') {
                        var dataSmooth = $scope.dataWs[dataTemp.iotid][$scope.dataWs[dataTemp.iotid].length - 1];
                        var ii = 0;
                        $interval(function() {


                            $scope.dataGraph[dataSmooth.iotid][0].push(Number(dataSmooth.luminosity));
                            $scope.dataGraph[dataSmooth.iotid][1].push(Number(dataSmooth.current * 10));
                            $scope.dataGraph[dataSmooth.iotid][2].push(Number(dataSmooth.temperature));
                            $scope.dataGraph[dataSmooth.iotid][3].push(Number(dataSmooth.humidity));
                            var dd = '';
                            var d = new Date();
                            dd = d.getHours() + ":" + d.getMinutes() + ":" + d.getSeconds();
                            $scope.labelsGraph[dataSmooth.iotid].push(dd);
                            $scope.dataRadar[dataSmooth.iotid][0] = [dataSmooth.luminosity, dataSmooth.current * 10, dataSmooth.temperature, dataSmooth.humidity];
                        }, 50, 1);

                    }


                    $scope.dataWs[dataTemp.iotid].push(dataTemp);
                } else {
                    $scope.dataWs[dataTemp.iotid][$scope.dataWs[dataTemp.iotid].length - 1].R1 = dataTemp.R1;
                }
            });
        };

        //$scope.startWebsocket2('192.168.1.32:81');
        //$scope.startWebsocket('192.168.1.31:81');
        $scope.closeWebsocket = function() {
            $scope.connected = 0;
            $scope.Messages.stopConnection();
        };
        $scope.scanServer = function() {
            console.log('scann');
            $http.get('http://78.218.62.97:8057/api/esps/scan/scan', { timeout: 10000 }).success(function(datas) {
                //console.log(datas);
            });
        };

        //$interval(function() {
        //$scope.scanServer();
        //}, 10000);

        $scope.scanNetwork = function() {
            /*$http.get('http://78.218.62.97:8057/api/esps/scan/scan', { timeout: 10000 }).success(function(datas) {
                console.log(datas);
            });*/
            for (var i = 0; i < 254; i++) {
                var ip = '192.168.1.' + i;
                try {
                    $http.get('http://' + ip + '/relay', { timeout: 1000 }).success(function(datas) {
                        $scope.goodNetwork = datas.localip + ':81';
                        $scope.theUrl = $scope.goodNetwork;
                        console.log('Launch Websocket - ' + $scope.goodNetwork);
                        $scope.dataWs[datas.iotid] = [];
                        $scope.dataRadar[datas.iotid] = [
                            [65, 59, 90, 81]
                        ];
                        $scope.dataGraph[datas.iotid] = [
                            [],
                            [],
                            [],
                            []
                        ];
                        $scope.labelsGraph[datas.iotid] = [];
                        $scope.onClick2[datas.iotid] = function(points, evt) {
                            console.log(points, evt);
                        };
                        $scope.startWebsocketS($scope.goodNetwork, datas.iotid);
                    }).error(function(err) {
                        console.log('No response from ' + ip);
                    }).catch(function(error) {
                        $scope.networkk.push(ip);
                        console.log(error.config.url);
                    });
                } catch (e) {
                    console.log(e);
                }
            };
        };


        $scope.capteurs.push({
            'function': 'sensor',
            'name': 'Température',
            'valeur': '22',
            'image': 'img/temp.svg',
            'code': 'r1'
        }, {
            'function': 'sensor',
            'name': 'Humidité',
            'valeur': '62',
            'image': 'img/hum.svg',
            'code': 'r2'
        }, {
            'function': 'sensor',
            'name': 'Luminosité',
            'valeur': '',
            'image': 'img/lux.svg',
            'code': 'r3'
        }, {
            'function': 'sensor',
            'name': 'Energie',
            'valeur': '80',
            'image': 'img/energy.svg',
            'code': 'r4'
        });

        $scope.relay = function(ip, value) {
            $http.get('http://' + ip + '/relay?r1=' + value).success(function(datas) {
                $scope.lesDatas = datas;
                //socket.sendMessage(datas);
                //console.log(datas)
            });
        };

        $scope.allumeS = function(relay, name) {
            $scope.mesSent = { 'R1': 1 };
            console.log('Sent to :' + name);
            if (name === 'dub8266') {
                $scope.allSockets.dub8266.send($scope.mesSent);
            }
            if (name === 'ConnectPlug') {
                $scope.allSockets.ConnectPlug.send($scope.mesSent);
            }
        };
        $scope.eteindsS = function(relay, name) {
            $scope.mesSent = { 'R1': 0 };
            console.log('Sent to :' + name);
            if (name === 'dub8266') {
                $scope.allSockets.dub8266.send($scope.mesSent);
            }
            if (name === 'ConnectPlug') {
                $scope.allSockets.ConnectPlug.send($scope.mesSent);
            }
        };
    })
    .config(function($mdThemingProvider) {
        $mdThemingProvider.theme('dark-grey').backgroundPalette('grey').dark();
        $mdThemingProvider.theme('dark-orange').backgroundPalette('orange').dark();
        $mdThemingProvider.theme('dark-purple').backgroundPalette('deep-purple').dark();
        $mdThemingProvider.theme('dark-blue').backgroundPalette('blue').dark();
    })
    .config(function($stateProvider) {
        $stateProvider
            .state('homepage', {
                url: '/',
                templateUrl: 'views/homepage.html',
                controller: 'AppCtrl'
            })
            .state('esp', {
                url: '/esp',
                templateUrl: 'views/esp.html',
                controller: 'AppCtrl'
            })
            .state('test', {
                url: '/test',
                templateUrl: 'views/test.html',
                controller: 'AppCtrl'
            });
    });


var loadingImage = false;

function LoadImage(imageName, imageFile) {
    if ((!document.images) || loadingImage) return;
    loadingImage = true;
    if (document.images[imageName].src.indexOf(imageFile) < 0) {
        document.images[imageName].src = imageFile;
    }
    loadingImage = false;
}
LoadImage('image0', 'img/energy.svg');