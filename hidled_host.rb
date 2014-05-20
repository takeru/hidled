require "libusb"

module HIDLED
  class Host
    include LIBUSB

    USBRQ_HID_GET_REPORT    = 0x01
    USBRQ_HID_GET_IDLE      = 0x02
    USBRQ_HID_GET_PROTOCOL  = 0x03
    USBRQ_HID_SET_REPORT    = 0x09
    USBRQ_HID_SET_IDLE      = 0x0a
    USBRQ_HID_SET_PROTOCOL  = 0x0b

    def open_handle
      usb = LIBUSB::Context.new

      devices = usb.devices(:idVendor => 0x16c0, :idProduct => 0x05dc)
      if devices.size==0
        raise "device not found."
      end

      device = devices.first
      device.open_interface(0) do |handle|
        yield(handle)
      end
    end

    # send to device
    def send_data(data)
      open_handle do |handle|
        sent_byte_size = handle.control_transfer(
          :bmRequestType => ENDPOINT_OUT|REQUEST_TYPE_CLASS|RECIPIENT_ENDPOINT,
          :bRequest      => USBRQ_HID_SET_REPORT,
          :wValue        => 0x0000,
          :wIndex        => 0x0000,
          :dataOut       => data.pack("C*")
        )
        sent_byte_size
      end
    end

    # receive from device
    def recv_data
      open_handle do |handle|
        received_bytes = handle.control_transfer(
          :bmRequestType => ENDPOINT_IN|REQUEST_TYPE_CLASS|RECIPIENT_ENDPOINT,
          :bRequest      => USBRQ_HID_GET_REPORT,
          :wValue        => 0x0000,
          :wIndex        => 0x0000,
          :dataIn        => 8
        )
        received_bytes.unpack('C*')
      end
    end

    def demo_send_and_recv
      16.times do |n|
        data = [n+0,n+1,n+2,n+3,n+4,n+5,n+6,n+7]

        sent_byte_size = send_data(data)
        puts "sent_byte_size = #{sent_byte_size}"

        data = recv_data
        puts "received_bytes = #{received_bytes.unpack('C*').inspect}"
      end
    end
  end
end

if $0 == File.basename(__FILE__)
  usage = true
  host = HIDLED::Host.new
  if ARGV[0]=="-r"
    puts host.recv_data.join(",")
    usage = false
  elsif ARGV[0]=="-s"
    data = ARGV[1].split(",").map{|s| s.strip.to_i }
    if data.size==8
      host.send_data(data)
      usage = false
    end
  end

  if usage
    puts "usage: "
    puts "  ruby hidled_host.rb -r"
    puts "  ruby hidled_host.rb -s 1,0,1,0,0,0,0,0"
  end
end
