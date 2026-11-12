# Cuatro robots patrullan su cuadrante; muros separan las zonas
world 40 40 {
  wall 19 0 2 40
  wall 0 19 19 2
  wall 21 19 19 2
  hole at 30 30
  robot r1 at 5 5 facing north
  robot r2 at 25 5 facing north
  robot r3 at 5 25 facing north
  robot r4 at 25 25 facing north
}
run r1 { repeat 250000 { forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 } }
run r2 { repeat 250000 { forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 } }
run r3 { repeat 250000 { forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 } }
run r4 { repeat 250000 { forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 forward 1 forward 1 right 90 } }
