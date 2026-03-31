/**
 * CSInterface - Adobe CEP Interface
 */
var CSInterface = function() {};

CSInterface.prototype.evalScript = function(script, callback) {
    if (callback === null || callback === undefined) {
        callback = function(result) {};
    }
    try {
        window.__adobe_cep__.evalScript(script, callback);
    } catch(e) {
        console.error('evalScript error:', e);
        if (callback) callback('EvalScript error: ' + e.toString());
    }
};

CSInterface.prototype.getSystemPath = function(pathType) {
    var path = window.__adobe_cep__.getSystemPath(pathType);
    return decodeURI(path);
};

CSInterface.prototype.closeExtension = function() {
    window.__adobe_cep__.closeExtension();
};

CSInterface.prototype.requestOpenExtension = function(extensionId) {
    window.__adobe_cep__.requestOpenExtension(extensionId);
};

CSInterface.prototype.addEventListener = function(type, listener, obj) {
    try {
        window.__adobe_cep__.addEventListener(type, listener, obj);
    } catch(e) {
        console.error('addEventListener error:', e);
    }
};

CSInterface.prototype.removeEventListener = function(type, listener, obj) {
    try {
        window.__adobe_cep__.removeEventListener(type, listener, obj);
    } catch(e) {
        console.error('removeEventListener error:', e);
    }
};

CSInterface.prototype.dispatchEvent = function(event) {
    try {
        if (typeof event.data === 'object') {
            event.data = JSON.stringify(event.data);
        }
        window.__adobe_cep__.dispatchEvent(event);
    } catch(e) {
        console.error('dispatchEvent error:', e);
    }
};

CSInterface.SystemPath = {
    EXTENSION: "extension",
    HOST_APPLICATION: "hostApplication"
};
