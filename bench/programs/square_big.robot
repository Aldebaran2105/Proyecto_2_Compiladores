# Un robot recorre 1 000 000 de veces un cuadrado de lado 3
world 10 10 {
  robot r1 at 2 2 facing north
}
run r1 {
  repeat 1000000 {
    forward 3
    right 90
  }
}
