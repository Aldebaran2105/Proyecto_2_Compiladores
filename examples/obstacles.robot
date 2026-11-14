world 10 8 {
  wall 4 3 2 2
  obstacle at 2 6
  hole at 7 2
  robot r1 at 1 3 facing east priority 1
  robot r2 at 7 6 facing south priority 2
  robot r3 at 2 1 facing north priority 3
}
run r1 {
  forward 5
}
run r2 {
  forward 6
}
run r3 {
  forward 6
}