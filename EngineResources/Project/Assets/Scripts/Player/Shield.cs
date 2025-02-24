using TheEngine;
using TheEngine.TheConsole;

public class Shield 
{
	public TheGameObject slave;
	private TheScript slave_script = null;
	
	public bool front_shield = false;
	public bool back_shield = false;

	
	void Start () 
	{
		if(slave != null)
			slave_script = slave.GetScript("PlayerMovement");

		TheConsole.Log(slave.name);
	}
	
	void Update () 
	{
		
	}
	

	void OnTriggerEnter(TheCollisionData coll)
	{
		
		if(coll == null)
			return;

		TheGameObject colision_object = coll.Collider.GetGameObject();

		if(colision_object == null)
			return;
		
		TheScript laser = colision_object.GetScript("Laser");
		
		TheGameObject sender = (TheGameObject)laser.CallFunctionArgs("GetSender");

		if(laser != null && slave_script != null && sender.GetComponent<TheTransform>() != slave.GetComponent<TheTransform>())
		{
			int dmg = (int)laser.CallFunctionArgs("GetDamage");
			object[] args = {dmg};
			colision_object.SetActive(false);
			
			if(front_shield && back_shield)
				TheConsole.Log("Invalid Shield Options");
			else if(front_shield)
				slave_script.CallFunctionArgs("DamageFrontShield", args);
			else if(back_shield)
				slave_script.CallFunctionArgs("DamageBackShield", args);
			else
				TheConsole.Log("Invalid Shield Options");
				
		}
		
	}
}