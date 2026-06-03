#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/crc8.h>
#include <linux/iio/iio.h>
#include <linux/delay.h>
#include "sht41.h"

/**
 * CRC8 table for SHT41 sensor.
 */
DECLARE_CRC8_TABLE(sht41_crc8_table);


/*************************** PRECISION MODES *************************************/

/**
 * Enum for the precision modes of the SHT41 sensor.
 */
enum precision_mode{
	MODE_HIGH_PRECISION = 0,
	MODE_MEDIUM_PRECISION,
	MODE_LOW_PRECISION,
};

/**
 * Maps precision Enum index to the actual hardware commands.
 * This is used to determine which command to send to the sensor when the user selects a precision mode.
 */
static const u8 sht41_precision_cmds[] = {
	[MODE_HIGH_PRECISION] = CMD_MEASURE_HIGH_PRECISION,
	[MODE_MEDIUM_PRECISION] = CMD_MEASURE_MEDIUM_PRECISION,
	[MODE_LOW_PRECISION] = CMD_MEASURE_LOW_PRECISION,
};

/**
 * Maps precision Enum index to user-friendly strings for sysfs attributes. ReadOnly data. 
 * This are the one presented to the user when they read the precision attribute.
 * The IIO core will use the index returned by get_precision to look up the corresponding string in this array and show it to the user.
 */
static const char *const sht41_precision_modes_usr_layer[] = {
	[MODE_HIGH_PRECISION] = "high",
	[MODE_MEDIUM_PRECISION] = "medium",
	[MODE_LOW_PRECISION] = "low",
};


/*************************** HEATER MODES *************************************/

/**
 * Enum for the heater power modes of the SHT41 sensor.
 */
enum heater_power_mode{
	HEATER_200mW = 0,
	HEATER_110mW,
	HEATER_20mW,
};


/**
 * Enum for the heater duration modes of the SHT41 sensor.
 */
enum heater_duration_mode{
	HEATER_DURATION_1S = 0,
	HEATER_DURATION_0_1S,
};

/**
 * Maps heater power Enum index to user-friendly strings for sysfs attributes. ReadOnly data. 
 * This are the one presented to the user when they read the heater_power attribute.
 */
static const char *const sht41_heater_power_modes_usr_layer[] = {
	[HEATER_200mW] = "200mW",
	[HEATER_110mW] = "110mW",
	[HEATER_20mW] = "20mW",
};

/**
 * Maps heater duration Enum index to user-friendly strings for sysfs attributes. ReadOnly data. 
 * This are the one presented to the user when they read the heater_duration attribute.
 */
static const char *const sht41_heater_duration_modes_usr_layer[] = {
	[HEATER_DURATION_1S] = "1s",
	[HEATER_DURATION_0_1S] = "0.1s",
};


/**
 * Maps heater power and duration Enum indices to the actual hardware commands.
 * This is used to determine which command to send to the sensor when the user selects a heater power and duration combination.
 */
static const u8 sht41_heater_power_cmds[3][2] = {
	[HEATER_200mW][HEATER_DURATION_1S] = CMD_HEATER_200mW_1S,
	[HEATER_200mW][HEATER_DURATION_0_1S] = CMD_HEATER_200mW_0_1S,
	[HEATER_110mW][HEATER_DURATION_1S] = CMD_HEATER_110mW_1S,
	[HEATER_110mW][HEATER_DURATION_0_1S] = CMD_HEATER_110mW_0_1S,
	[HEATER_20mW][HEATER_DURATION_1S] = CMD_HEATER_20mW_1S,
	[HEATER_20mW][HEATER_DURATION_0_1S] = CMD_HEATER_20mW_0_1S,
};




/**
 * Private data structure for the SHT41 sensor driver.
 * This structure holds all the necessary information for managing the sensor, including its I2C client, serial number, and measurement data.
 */
struct sht41_data{
	struct i2c_client *client;
	u32 serial_number;
	struct mutex lock;
	u16 temperature;
	u16 humidity;
	u8 mode_precision;
	u8 heater_power;
	u8 heater_duration;
};

/**
 * Sets the precision mode of the SHT41 sensor based on user input from sysfs attributes.
 * @param indio_dev: IIO device structure
 * @param chan: Channel specification (not used in this function but required by the IIO enum set function signature)
 * @param val: The precision mode index selected by the user (0 for high, 1 for medium, 2 for low)
 * @return: 0 on success, negative error code on failure
 */
static int sht41_set_precision(struct iio_dev *indio_dev, const struct iio_chan_spec *chan, unsigned int val){
	struct sht41_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);

	if (val < 0 || val >= ARRAY_SIZE(sht41_precision_cmds)){
		mutex_unlock(&data->lock);
		return -EINVAL;
	}

	data->mode_precision = (u8)val;
	mutex_unlock(&data->lock);

	return 0;
}

/**
* Gets the current precision mode of the SHT41 sensor to present it to the user through sysfs attributes.
* The Logic Flow
1.  User runs cat precision.
2.  IIO Core finds your get function.
3.  IIO Core packs up the indio_dev, any private data, and the chan info.
4.  The driver (this function) runs, looks at data->mode_precision, and decides the current index is 1.
5.  The driver writes 1 into *val and returns 0 (success).
6.  IIO Core sees the 1, finds "medium" in your string array, and shows it to the user.
  Since mode_precision is a single byte, the read is atomic at the hardware level. However, used the mutex for consistency.
* @param indio_dev: IIO device structure
* @param chan: Channel specification (not used in this function but required by the IIO enum get function signature)
* @return: The current precision mode index (0 for high, 1 for medium, 2 for low) on success, negative error code on failure
*/
static int sht41_get_precision(struct iio_dev *indio_dev, const struct iio_chan_spec *chan){

	struct sht41_data *data = iio_priv(indio_dev);
	int val_precision;

	// Lock the mutex to safely read the mode_precision value from the driver's private data structure.
	mutex_lock(&data->lock);
	val_precision = (int)data->mode_precision;
	mutex_unlock(&data->lock);

	return val_precision;
}


	

static ssize_t heater_power_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t len){
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sht41_data *data = iio_priv(indio_dev);

	int i;

	for(i=0; i < ARRAY_SIZE(sht41_heater_power_modes_usr_layer); i++){
		if (strcmp(buf, sht41_heater_power_modes_usr_layer[i]) == 0){
			mutex_lock(&data->lock);
			data->heater_power = i;
			mutex_unlock(&data->lock);
			return len;
		}
	}
	return -EINVAL;
}


static ssize_t heater_power_show (struct device *dev, struct device_attribute *attr, char *buf){
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sht41_data *data = iio_priv(indio_dev);
	int val_heater_power;

	mutex_lock(&data->lock);
	val_heater_power = (int)data->heater_power;
	mutex_unlock(&data->lock);

	return sysfs_emit(buf, "%s\n", sht41_heater_power_modes_usr_layer[val_heater_power]);
}


static ssize_t heater_power_available_show (struct device *dev, struct device_attribute *attr, char *buf){
	return sysfs_emit(buf, "200mW 110mW 20mW\n");
}

static ssize_t heater_duration_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t len){
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sht41_data *data = iio_priv(indio_dev);

	int i;

	for(i=0; i < ARRAY_SIZE(sht41_heater_duration_modes_usr_layer); i++){
		if (strcmp(buf, sht41_heater_duration_modes_usr_layer[i]) == 0){
			mutex_lock(&data->lock);
			data->heater_duration = i;
			mutex_unlock(&data->lock);
			return len;
		}
	}
	return -EINVAL;
}


static ssize_t heater_duration_show (struct device *dev, struct device_attribute *attr, char *buf){
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sht41_data *data = iio_priv(indio_dev);
	int val_heater_duration;

	mutex_lock(&data->lock);
	val_heater_duration = (int)data->heater_duration;
	mutex_unlock(&data->lock);

	return sysfs_emit(buf, "%s\n", sht41_heater_duration_modes_usr_layer[val_heater_duration]);
}


static ssize_t heater_duration_available_show (struct device *dev, struct device_attribute *attr, char *buf){
	return sysfs_emit(buf, "1s 0.1s\n");
}


static ssize_t heater_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {

	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sht41_data *data = iio_priv(indio_dev);
	bool trigger_heater;
	u8 cmd;
	int ret;

	
	ret = kstrtobool(buf,&trigger_heater);
	if (ret){
		return ret;
	}
	
	if(trigger_heater){
		
		mutex_lock(&data->lock);
		cmd = sht41_heater_power_cmds[data->heater_power][data->heater_duration];
		
		ret = i2c_master_send(data->client, &cmd, 1);
		if (ret < 0){
			mutex_unlock(&data->lock);
			dev_err(&data->client->dev, "Failed to send heater command: %d\n", ret);
			return ret;
		}
		else if (ret != 1){
			mutex_unlock(&data->lock);
			dev_err(&data->client->dev, "Unexpected number of bytes sent for heater command: %d\n", ret);
			return -EIO;
		}


		// From cheat-sheet -> wait for the heater to complete its cycle based on the selected duration to ensure completion.
		if(data->heater_duration == HEATER_DURATION_1S){
			msleep(1100); 
		}
		else{
			msleep(200);
		}
		mutex_unlock(&data->lock);
	}

	return len;
}

static ssize_t serial_number_show (struct device *dev, struct device_attribute *attr, char *buf){
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
    struct sht41_data *data = iio_priv(indio_dev);

    return sysfs_emit(buf, "0x%08x\n", data->serial_number);
}

static ssize_t soft_reset_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len){
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct sht41_data *data = iio_priv(indio_dev);
	bool trigger_reset;
	u8 cmd = CMD_SOFT_RESET;
	int ret;

	ret = kstrtobool(buf, &trigger_reset);
	if (ret){
		return ret;
	}

	if (trigger_reset){
		mutex_lock(&data->lock);
		ret = i2c_master_send(data->client, &cmd, 1);
		if (ret < 0){
			mutex_unlock(&data->lock);
			dev_err(&data->client->dev, "Failed to send soft reset command: %d\n", ret);
			return ret;
		}
		else if (ret != 1){
			mutex_unlock(&data->lock);
			dev_err(&data->client->dev, "Unexpected number of bytes sent for soft reset command: %d\n", ret);
			return -EIO;
		}

		msleep(20);
		mutex_unlock(&data->lock);
		dev_info(&data->client->dev, "SHT41: soft reset triggered via sysfs\n"); 
	}

	return len;

}




static DEVICE_ATTR_RW(heater_power);
static DEVICE_ATTR_RO(heater_power_available);
static DEVICE_ATTR_RW(heater_duration);
static DEVICE_ATTR_RO(heater_duration_available);
static DEVICE_ATTR_WO(heater_enable);
static DEVICE_ATTR_RO(serial_number);
static DEVICE_ATTR_WO(soft_reset);

static struct attribute *sht41_attributes[] = {
    &dev_attr_heater_power.attr,
    &dev_attr_heater_power_available.attr,
    &dev_attr_heater_duration.attr,
    &dev_attr_heater_duration_available.attr,
    &dev_attr_heater_enable.attr,
    &dev_attr_serial_number.attr,
    &dev_attr_soft_reset.attr,
    NULL
};

static const struct attribute_group sht41_attribute_group = {
    .attrs = sht41_attributes,
};



/**
 * Checks the CRC of the received data.
 * @param data: Pointer to the data buffer that was received from the sensor (excluding the CRC byte)
 * @param len: Length of the data buffer (in bytes)
 * @param checksum: Expected checksum value that was received from the sensor (the CRC byte)
 * @return: 0 on success, negative error code on failure
 */
static int sht41_check_crc(u8 *data, size_t len, u8 checksum){
	u8 res;

	res = crc8(sht41_crc8_table, data, len, CRC8_INIT_VALUE);
	if (res != checksum){
		pr_err("CRC check failed: calculated 0x%02x, expected 0x%02x\n", res, checksum);
		return EBADMSG;
	}

	return 0;
}

/**
 * Reads a measurement from the SHT41 sensor, including both temperature and humidity data, and stores it in the driver's private data structure.
 * This function sends the appropriate command to the sensor based on the current precision mode, waits for the measurement to be taken,
 * reads the response, checks the CRC, and extracts the raw temperature and humidity values.
 * @param data: Pointer to the driver's private data structure
 * @return: 0 on success, negative error code on failure
 */
static int sht41_read_measurement(struct sht41_data *data){

	u8 cmd;
	u8 response[READ_RESPONSE_LENGTH];
	int ret;

	mutex_lock(&data->lock);

	// Determine the command to send based on the current precision mode.
	cmd = sht41_precision_cmds[data->mode_precision];

	// Send the measurement command to the sensor.
	ret = i2c_master_send(data->client, &cmd, 1);
	if (ret < 0){
		mutex_unlock(&data->lock);
		dev_err(&data->client->dev, "Failed to send measurement command: %d\n", ret);
		return ret;
	}
	else if (ret != 1){
		mutex_unlock(&data->lock);
		dev_err(&data->client->dev, "Unexpected number of bytes sent for measurement command: %d\n", ret);
		return -EIO;
	}

	/* Dynamically sleep based on active precision mode datasheet requirements */
    if (data->mode_precision == MODE_HIGH_PRECISION) {
        msleep(10);  	/* Max 8.3ms requirement */
    } else if (data->mode_precision == MODE_MEDIUM_PRECISION) {
        msleep(6);   	/* Max 4.5ms requirement */
    } else {
        msleep(4);   	/* Give low precision (2.3ms) a safe, relaxed window to settle */
	}

	// Read the measurement response from the sensor.
	ret = i2c_master_recv(data->client, response, READ_RESPONSE_LENGTH);
	if (ret != READ_RESPONSE_LENGTH){
		mutex_unlock(&data->lock);
		dev_err(&data->client->dev, "Failed to read measurement response: %d\n", ret);
		return (ret < 0) ? ret : -EIO;
	}

	// Verify the CRC for both temperature and humidity data.
	if (sht41_check_crc(&response[0], 2, response[2]) ||
		sht41_check_crc(&response[3], 2, response[5])){
		mutex_unlock(&data->lock);
		dev_err(&data->client->dev, "Measurement CRC failed\n");
		return -EBADMSG;
	}

	// Store the raw temperature and humidity values in the driver's private data structure for later retrieval by the read_raw function.
	data->temperature = (response[0] << 8) | response[1];
	data->humidity = (response[3] << 8) | response[4];
	mutex_unlock(&data->lock);

	return 0;
}

/**
 * Reads the raw value from the SHT41 sensor.
 * @param indio_dev: IIO device structure
 * @param chan: Channel specification
 * @param val: Pointer to store the raw value
 * @param val2: Pointer to store the second value (if applicable)
 * @param mask: Mask for the read operation
 * @return: 0 on success, negative error code on failure
 */
static int sht41_read_raw(struct iio_dev *indio_dev, struct iio_chan_spec const *chan, int *val, int *val2, long mask){

	struct sht41_data *data = iio_priv(indio_dev);
	int ret;

	switch (mask){
	case IIO_CHAN_INFO_RAW:

		// Read raw data from the sensor based on the current precision mode.
		ret = sht41_read_measurement(data);
		if (ret)
			return ret;

		if (chan->type == IIO_TEMP){
			*val = (int)data->temperature;
		}else if (chan->type == IIO_HUMIDITYRELATIVE){
			*val = (int)data->humidity;
		}else{
			return -EINVAL;
		}

		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:

		// Return the scale factor for the channel based on the sensor's datasheet.
		// This is used by the IIO core to convert raw values to real-world units.
		// By returning IIO_VAL_FRACTIONAL kernel's IIO core safely perform the division and format it as a decimal string for userspace.
		// This keeps the kernel driver fast and free of dangerous floating-point arithmetic.
		if (chan->type == IIO_TEMP){
			*val = 175000; // Scale factor for temperature milli-degrees Celsius (0.01 °C per LSB, so 10000 in fixed-point representation)
			*val2 = 65535; // Denominator for fixed-point representation

			return IIO_VAL_FRACTIONAL;
		}
		else if (chan->type == IIO_HUMIDITYRELATIVE){
			*val = 125000; // Scale factor for humidity milli-%RH (0.01 %RH per LSB, so 10000 in fixed-point representation)
			*val2 = 65535; // Denominator for fixed-point representation

			return IIO_VAL_FRACTIONAL;
		}
		else{
			return -EINVAL;
		}

	case IIO_CHAN_INFO_OFFSET:

		// Return the offset for the channel based on the sensor's datasheet.
		// This is used by the IIO core to convert raw values to real-world units.
		if (chan->type == IIO_TEMP){
			*val = -16852; // offset for temperature (-45 * 65535) / 175

			return IIO_VAL_INT;
		}
		else if (chan->type == IIO_HUMIDITYRELATIVE){
			*val = -3146; // offset for humidity (-6 * 65535) / 125

			return IIO_VAL_INT;
		}
		else{
			return -EINVAL;
		}

	default:
		return -EINVAL;
	}

	return 0;
}



/**
 * Enumeration structure for the precision modes of the SHT41 sensor. This structure defines the available precision modes,
 * the number of modes, and the get and set functions that are used to read and write the precision mode from sysfs attributes.
 * The IIO core uses this structure to create the appropriate sysfs attributes for controlling the precision mode of the sensor.
 */
static const struct iio_enum sht41_precision_enum = {
	.items = sht41_precision_modes_usr_layer,
	.num_items = ARRAY_SIZE(sht41_precision_modes_usr_layer),
	.get = sht41_get_precision,
	.set = sht41_set_precision,
};




/**
 * Extended channel information for the SHT41 sensor. This defines additional attributes for the channels, such as the precision mode,
 * and links them to the corresponding get and set functions in the driver.
 */
static const struct iio_chan_spec_ext_info sht41_ext_info[] = {
	IIO_ENUM("precision", IIO_SHARED_BY_ALL, &sht41_precision_enum),
	{
        .name = "precision_available",
        .shared = IIO_SHARED_BY_ALL,              /* <-- This stops the channel duplication! */
        .read = iio_enum_available_read,
        .private = (uintptr_t)&sht41_precision_enum, /* Standard enum pointer mapping */
    },
	
	//IIO_ENUM_AVAILABLE("precision", &sht41_precision_enum),

	{}
};

/**
 * Channel specification for the SHT41 sensor. This defines the channels that the sensor provides (temperature and humidity),
 * the type of data they represent, and the information that can be read or written for each channel.
 * The IIO core uses this specification to create the appropriate sysfs attributes and to route read and write requests to the
 *  correct functions in the driver.
 */
static const struct iio_chan_spec sht41_channels[] = {
	{
		.type = IIO_TEMP,
		// Create files: in_temp_raw, in_temp_scale, and in_temp_offset
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
							  BIT(IIO_CHAN_INFO_SCALE) |
							  BIT(IIO_CHAN_INFO_OFFSET),
		// Attach the precision enum, creating precision and precision_available sysfs attributes.
		.ext_info = sht41_ext_info,
	},
	{
		.type = IIO_HUMIDITYRELATIVE,
		// Create files: in_humidityrelative_raw, in_humidityrelative_scale, and in_humidityrelative_offset
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
							  BIT(IIO_CHAN_INFO_SCALE) |
							  BIT(IIO_CHAN_INFO_OFFSET),
		// Attach the precision enum, creating precision and precision_available sysfs attributes.
		.ext_info = sht41_ext_info,
	}};

static const struct iio_info sht41_info = {
	.read_raw = sht41_read_raw,
	.attrs = &sht41_attribute_group,
};

/**
 * Reads the serial number from the SHT41 sensor.
 * @param client: I2C client structure
 * @param serial: Pointer to store the serial number
 * @return: 0 on success, negative error code on failure
 */
static int sht41_read_serial(struct i2c_client *client, u32 *serial){
	u8 buf[6];
	int ret;

	// Send READ SERIAL COMMAND
	ret = i2c_master_send(client, (u8[]){CMD_READ_SERIAL_NUMBER}, 1);
	if (ret < 0)
		return ret;

	// Read the 6-byte response
	ret = i2c_master_recv(client, buf, 6);
	if (ret != 6)
		return (ret < 0) ? ret : -EIO;

	// Verify CRCs for both 16-bit words
	if (sht41_check_crc(&buf[0], 2, buf[2]) || sht41_check_crc(&buf[3], 2, buf[5])) {
		dev_err(&client->dev, "Serial number CRC failed\n");
		return -EBADMSG;
	}

	// Combine the 4 data bytes into a single 32-bit number
	// Formula: (D0 << 24) | (D1 << 16) | (D3 << 8) | D4
	*serial = (buf[0] << 24) | (buf[1] << 16) | (buf[3] << 8) | buf[4];

	return 0;
}

/**
 * Probe function for the SHT41 driver. This function is called when the I2C core detects a device that matches the driver's ID table.
 * It initializes the device, sets up the IIO device structure, and registers the device with the IIO subsystem.
 * @param client: I2C client structure
 * @param id: I2C device ID structure
 * @return: 0 on success, negative error code on failure
 */
static int sht41_probe(struct i2c_client *client, const struct i2c_device_id *id){

	struct device *dev = &client->dev;
	struct iio_dev *indio_dev;
	struct sht41_data *data;
	int ret;
	int retry;

	// check if i2c functionality is supported
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)){
		dev_err(dev, "I2C functionality not supported by adapter\n");
		return -EIO;
	}

	/* Allocate IIO device with private data for our driver. The IIO core will manage the memory for this device and its private data, \
	 * and will automatically free it when the device is removed.
	 */
	indio_dev = devm_iio_device_alloc(dev, sizeof(struct sht41_data));
	if (!indio_dev){
		dev_err(dev, "Failed to allocate IIO device\n");
		return -ENOMEM;
	}

	data = iio_priv(indio_dev);
	data->client = client;
	mutex_init(&data->lock);

	i2c_set_clientdata(client, indio_dev);

	indio_dev->name = "sht41";
	indio_dev->info = &sht41_info;
	indio_dev->channels = sht41_channels;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->num_channels = ARRAY_SIZE(sht41_channels);

	// populate the CRC8 table using the specified polynomial
	crc8_populate_msb(sht41_crc8_table, CRC8_POLYNOMIAL);

	// Wait for the sensor to be ready after power-up.
	// This is a safe value that ensures the sensor has completed its internal initialization before we start communicating with it. (by cheat sheet 1ms is enough).
	msleep(10);
	/**
	 * HW TEST : sending SOFT RESET command to perform a soft reset of the sensor
	 * to ensure it is in a known state before we start communicating with it.
	 * The retry mechanism is implemented to handle potential USB/I2C power-up glitches.
	 */
	for (retry = 0; retry < 3; retry++){
		ret = i2c_master_send(client, (u8[]){CMD_SOFT_RESET}, 1);

		if (ret == 1){
			break; // success, exit the retry loop
		}
	}

	if (ret < 0){
		dev_err(dev, "Failed to send soft reset command: %d\n", ret);
		return ret;
	}
	if (ret != 1){
		dev_err(dev, "Unexpected number of bytes sent for soft reset command: %d\n", ret);
		return -EIO;
	}

	// CRITICAL: Waiting for the sensor to reboot before talking to it again
	msleep(10);

	/**
	 * Read SERIAL NUMBER to verify communication with the sensor and to retrieve the unique identifier of the sensor.
	 * This also serves as a test to ensure that we can successfully read data from the sensor and that the CRC checks are working correctly.
	 * The serial number is stored in the driver's private data structure
	 */
	ret = sht41_read_serial(client, &data->serial_number);
	if (ret){
		dev_err(dev, "Failed to read serial number: %d\n", ret);
		return ret;
	}
	dev_info(dev, "SHT41 serial number: 0x%08x\n", data->serial_number);

	ret = devm_iio_device_register(dev, indio_dev);
	if (ret){
		dev_err(dev, "Failed to register IIO device: %d\n", ret);
		return ret;
	}
	dev_info(dev, "SHT41 IIO driver successfully probed!\n");
	dev_info(dev, "driver initialized at address 0x%02x\n", client->addr);

	return 0;
}

/**
 * Structure for matching the device tree compatible string with the driver.
 * This allows the kernel to automatically load the driver when a device with the compatible string is detected on the I2C bus.
 * */
static const struct i2c_device_id sht41_id[] = {
	{"sht41", 0},
	{}};
MODULE_DEVICE_TABLE(i2c, sht41_id);

/**
 * Structure for registering the I2C driver with the kernel.
 * It specifies the driver's name, owner, probe and remove functions, and the device ID table for matching devices.
 * */
static struct i2c_driver sht41_driver = {
	.driver = {
		.name = "sht41",
		.owner = THIS_MODULE,
	},
	.probe = sht41_probe,
	.id_table = sht41_id,
};

module_i2c_driver(sht41_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Christian Lisi");
MODULE_DESCRIPTION("Linux Kernel Module for SHT41 sensor.");