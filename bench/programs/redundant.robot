# Programa "ingenuo": avanza de a una celda y gira de más
world 20 20 {
  robot r1 at 0 0 facing east
}
run r1 {
  repeat 250000 {
    forward 1
    forward 1
    forward 1
    right 90
    right 90
    forward 1
    forward 1
    forward 1
    right 90
    left 90
    right 90
    right 90
  }
}
