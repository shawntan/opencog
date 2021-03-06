package javadestin;


public interface Presentor {
	public void present();
	
	public void setNetwork(Network n);
	
	public void setSource(Source s);
	
	public void setInputTransporter(Transporter t);
	
	/**
	 * stops grabbing from the source and stops presenting.
	 * It is resumed again by calling present(); 
	 */
	public void stop();
	
	/**
	 * threads should use the returned object to synchronize on
	 * if they want to do something with the network while
	 * the presentor is presenting it, like reset its centroids.
	 * 
	 * synchronize(getNetworkLock()){
	 * 		... do stuff on network...
	 * }
	 * @return object to synchronize on
	 */
	public Object getNetworkLock();
	
}
