world 10 8 {
  wall 4 3 2 2
  hole at 7 2
  robot r1 at 1 1 facing east priority 1
  robot r2 at 8 1 facing west priority 2
}
run r1 {
  forward 3
  right 90
  forward 2
}
run r2 {
  forward 6
}
