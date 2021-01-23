import RPi.GPIO as GPIO
import os
import subprocess
from http.server import BaseHTTPRequestHandler, HTTPServer

host_name = '10.10.10.99' # my Raspberry Pi IP add ress 
host_port = 8000

class MyServer(BaseHTTPRequestHandler):
	""" A special implementation of BaseHTTPRequestHander for reading data from
	    and control GPIO of a Raspberry Pi
	"""
	def do_HEAD(self):
		""" do_HEAD() can be tested use curl command
		'curl -I http://server-ip-address:port'
		"""
		self.send_response(200)
		self.send_header('Content-type', 'text/html')
		self.end_headers()
	def _redirect(self, path):
		self.send_response(303)
		self.send_header('Content-type', 'text/html')
		self.send_header('Location', path)
		self.end_headers()
	def do_GET(self):
		""" do_GET() can be tested using curl command
			'curl http://server-ip-address:port'
		"""
		html = '''
			<html>
			<body style="width:960px; margin: 20px auto;">
			<h1>Welcome to Fahle home Automation page</h1>
			<p>Current GPU temperature is {}</p>
			<form action="/" method="POST">
				<label style="50px;width:200px;font-size:large" for="days">Days to replay (1 to 10):</label>
				<input style="height:50px;width:200px;font-size:large" type="number" id="days" name="days" min="1" max="10">
				<input style="height:50px;width:200px;font-size:large" type="submit" name="submit" value="Ok">
				<input style="height:50px;width:200px;font-size:large" type="submit" name="submit" value="Clear">
			</form>
			</body>
			</html>
		'''
		temp = os.popen("/opt/vc/bin/vcgencmd measure_temp").read()
		self.do_HEAD()
		self.wfile.write(html.format(temp[5:]).encode("utf-8"))
	def do_POST(self):
		""" do_POST() can be tested using curl command
			'curl -d "submit=On" http://server-ip-address:port'
		"""
		content_length = int(self.headers['Content-Length']) # Get the size of data
		post_data = self.rfile.read(content_length).decode("utf-8") # Get the data
		submit_data = post_data.split("submit=")[1] # Only keep the value
		days_data = post_data.split("&")[0] #only keep the days
		days_data = days_data.split("=")[1] #only keep the value
		print("Days to repeat {}".format(days_data)) #put out the days so we know
		if submit_data == 'Ok':
			subprocess.call(['sh', './replay.sh', days_data])
		else:
			subprocess.call(['sh', './lightsoff.sh'])
			
		print("LED is {}".format(submit_data))
		self._redirect('/') # Redirect back to the root url
	
if __name__ == '__main__':
	http_server = HTTPServer((host_name, host_port), MyServer)
	print("Server Starts - %s:%s" % (host_name, host_port))
	try:
		http_server.serve_forever()
	except KeyboardInterrupt:
		http_server.server_close()
	
