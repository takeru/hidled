require "libusb"

include LIBUSB

USBRQ_HID_GET_REPORT    = 0x01
USBRQ_HID_GET_IDLE      = 0x02
USBRQ_HID_GET_PROTOCOL  = 0x03
USBRQ_HID_SET_REPORT    = 0x09
USBRQ_HID_SET_IDLE      = 0x0a
USBRQ_HID_SET_PROTOCOL  = 0x0b

usb = LIBUSB::Context.new

devices = usb.devices(:idVendor => 0x16c0, :idProduct => 0x05dc)
puts "devices = #{devices.inspect}"
exit if devices.size==0

device = devices.first
device.open_interface(0) do |handle|
  16.times do |n|
    # send to device
    data = [n+0,n+1,n+2,n+3,n+4,n+5,n+6,n+7]

    sent_byte_size = handle.control_transfer(
      :bmRequestType => ENDPOINT_OUT|REQUEST_TYPE_CLASS|RECIPIENT_ENDPOINT,
      :bRequest      => USBRQ_HID_SET_REPORT,
      :wValue        => 0x0000,
      :wIndex        => 0x0000,
      :dataOut       => data.pack("C*")
    )
    puts "sent_byte_size = #{sent_byte_size}"

    # receive from device
    received_bytes = handle.control_transfer(
      :bmRequestType => ENDPOINT_IN|REQUEST_TYPE_CLASS|RECIPIENT_ENDPOINT,
      :bRequest      => USBRQ_HID_GET_REPORT,
      :wValue        => 0x0000,
      :wIndex        => 0x0000,
      :dataIn        => 8
    )
    puts "received_bytes = #{received_bytes.unpack('C*').inspect}"
  end
end
