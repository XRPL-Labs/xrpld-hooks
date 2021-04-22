"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
function setPrototypeOf(object, prototype) {
    Object.setPrototypeOf
        ? Object.setPrototypeOf(object, prototype)
        :
            (object.__proto__ = prototype);
}
exports.setPrototypeOf = setPrototypeOf;
function getConstructorName(object) {
    if (object.constructor.name) {
        return object.constructor.name;
    }
    const constructorString = object.constructor.toString();
    const functionConstructor = constructorString.match(/^function\s+([^(]*)/);
    const classConstructor = constructorString.match(/^class\s([^\s]*)/);
    return functionConstructor ? functionConstructor[1] : classConstructor[1];
}
exports.getConstructorName = getConstructorName;
//# sourceMappingURL=browser-hacks.js.map