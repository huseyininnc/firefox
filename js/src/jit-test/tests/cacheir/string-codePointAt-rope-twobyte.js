function makeString(from) {
  var codePoints = [];
  for (var i = 0; i < 16; ++i) {
    codePoints.push(from + i);
  }
  return String.fromCodePoint(...codePoints);
}

function makeRope() {
  var left = newRope(makeString(0x140), makeString(0x140 + 16));
  var right = newRope(makeString(0x160), makeString(0x160 + 16));
  var rope = newRope(left, right);
  return {left, right, rope};
}

// Load a character from the left rope child using a constant index. The input
// to String.prototype.codePointAt is always rope.
function testLeftChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var {rope} = makeRope();

    var ch = rope.codePointAt(0);
    assertEq(ch, 0x140);
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildConstant();
}

// Load a character from the right rope child using a constant index. The input
// to String.prototype.codePointAt is always rope.
function testRightChildConstant() {
  for (var i = 0; i < 200; ++i) {
    var {rope} = makeRope();

    var ch = rope.codePointAt(32);
    assertEq(ch, 0x160);
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildConstant();
}

// Load a character from the left rope child using a variable index. The input
// to String.prototype.codePointAt is always rope.
function testLeftChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var {left, rope} = makeRope();

    var idx = i % left.length;
    var ch = rope.codePointAt(idx);
    assertEq(ch, 0x140 + idx);
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildVariable();
}

// Load a character from the right rope child using a variable index. The input
// to String.prototype.codePointAt is always rope.
function testRightChildVariable() {
  for (var i = 0; i < 200; ++i) {
    var {left, right, rope} = makeRope();

    var idx = i % right.length;
    var ch = rope.codePointAt(left.length + idx);
    assertEq(ch, 0x160 + idx);
  }
}
for (var i = 0; i < 2; ++i) {
  testRightChildVariable();
}

// Load all characters from both child ropes. This covers the case when the
// call to String.prototype.codePointAt linearizes the rope. 
function testBothChildren() {
  for (var i = 0; i < 200; ++i) {
    var {rope} = makeRope();

    for (var j = 0; j < rope.length; ++j) {
      var ch = rope.codePointAt(j);
      assertEq(ch, 0x140 + j);
    }
  }
}
for (var i = 0; i < 2; ++i) {
  testBothChildren();
}

// Load a character from the left rope child using an absent index. The input
// to String.prototype.codePointAt is always rope.
function testLeftChildAbsentIndex() {
  for (var i = 0; i < 200; ++i) {
    var {rope} = makeRope();

    var ch = rope.codePointAt(/* ToInteger(ToNumber(undefined)) = ToInteger(NaN) = 0 */);
    assertEq(ch, 0x140);
  }
}
for (var i = 0; i < 2; ++i) {
  testLeftChildAbsentIndex();
}
