print "Hello world"

if os.isadmin() then
  print "I'm admin"
else
  print "I'm NOT admin"
end

if os.iswow64() then
  print "I'm a WOW64 process"
else
  print "I'm NOT a WOW64 process"
end
